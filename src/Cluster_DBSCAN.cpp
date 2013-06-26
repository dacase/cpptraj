#include <cfloat> // DBL_MAX
#include <algorithm> // sort, unique
#include "Cluster_DBSCAN.h"
#include "CpptrajStdio.h"
#include "ProgressBar.h"
#ifdef _OPENMP
#  include "omp.h"
#endif

Cluster_DBSCAN::Cluster_DBSCAN() :
  minPoints_(-1),
  epsilon_(-1.0),
  sieveToCentroid_(true)
{}

void Cluster_DBSCAN::Help() {
  mprintf("\t[dbscan minpoints <n> epsilon <e> [sievetoframe]]\n");
}

int Cluster_DBSCAN::SetupCluster(ArgList& analyzeArgs) {
  minPoints_ = analyzeArgs.getKeyInt("minpoints", -1);
  if (minPoints_ < 1) {
    mprinterr("Error: DBSCAN requires minimum # of points to be set and >= 1\n"
              "Error: Use 'minpoints <N>'\n");
    return 1;
  }
  epsilon_ = analyzeArgs.getKeyDouble("epsilon", -1.0);
  if (epsilon_ <= 0.0) {
    mprinterr("Error: DBSCAN requires epsilon to be set and > 0.0\n"
              "Error: Use 'epsilon <e>'\n");
    return 1;
  }
  sieveToCentroid_ = !analyzeArgs.hasKey("sievetoframe");
  return 0;
}

void Cluster_DBSCAN::ClusteringInfo() {
  mprintf("\tDBSCAN:\n");
  mprintf("\t\tMinimum pts to form cluster= %i\n", minPoints_);
  mprintf("\t\tCluster distance criterion= %.3f\n", epsilon_);
  if (sieveToCentroid_)
    mprintf("\t\tSieved frames will be added back solely based on their"
            " closeness to cluster centroids.\n"
            "\t\t  (This option is less accurate but faster.)\n");
  else
    mprintf("\t\tSieved frames will only be added back if they are within"
            " %.3f of a frame in an existing cluster.\n"
            "\t\t  (This option is more accurate and will identify sieved"
            " frames as noise but is slower.)\n", epsilon_);
}

void Cluster_DBSCAN::RegionQuery(std::vector<int>& NeighborPts,
                                 std::vector<int> const& FramesToCluster,
                                 int point)
{
  NeighborPts.clear();
  for (std::vector<int>::const_iterator otherpoint = FramesToCluster.begin();
                                        otherpoint != FramesToCluster.end();
                                        ++otherpoint)
  {
    if (point == *otherpoint) continue;
    if ( FrameDistances_.GetElement(point, *otherpoint) < epsilon_ )
      NeighborPts.push_back( *otherpoint );
  }
}

// Potential frame statuses.
char Cluster_DBSCAN::UNASSIGNED = 'U';
char Cluster_DBSCAN::NOISE = 'N';
char Cluster_DBSCAN::INCLUSTER = 'C';

/** Ester, Kriegel, Sander, Xu; Proceedings of 2nd International Conference
  * on Knowledge Discovery and Data Mining (KDD-96); pp 226-231.
  */
int Cluster_DBSCAN::Cluster() {
  std::vector<int> NeighborPts;
  std::vector<int> Npts2; // Will hold neighbors of a neighbor
  std::vector<int> FramesToCluster;
  ClusterDist::Cframes cluster_frames;
  // First determine which frames are being clustered.
  for (int frame = 0; frame < (int)FrameDistances_.Nframes(); ++frame)
    if (!FrameDistances_.IgnoringRow( frame ))
      FramesToCluster.push_back( frame );
  // Set up array to keep track of points that have been visited.
  // Make it the size of FrameDistances so we can index into it. May
  // waste memory during sieving but makes code easier.
  std::vector<bool> Visited( FrameDistances_.Nframes(), false );
  // Set up array to keep track of whether points are noise or in a cluster.
  Status_.assign( FrameDistances_.Nframes(), UNASSIGNED);
  mprintf("\tStarting DBSCAN Clustering:\n");
  ProgressBar cluster_progress(FramesToCluster.size());
  int iteration = 0;
  for (std::vector<int>::iterator point = FramesToCluster.begin();
                                  point != FramesToCluster.end(); ++point)
  {
    if (!Visited[*point]) {
      // Mark this point as visited
      Visited[*point] = true;
      // Determine how many other points are near this point
      RegionQuery( NeighborPts, FramesToCluster, *point );
      if (debug_ > 0) {
        mprintf("\tPoint %i\n", *point + 1);
        mprintf("\t\t%u neighbors:", NeighborPts.size());
      }
      // If # of neighbors less than cutoff, noise; otherwise cluster
      if ((int)NeighborPts.size() < minPoints_) {
        if (debug_ > 0) mprintf(" NOISE\n");
        Status_[*point] = NOISE;
      } else {
        // Expand cluster
        cluster_frames.clear();
        cluster_frames.push_back( *point );
        // NOTE: Use index instead of iterator since NeighborPts may be
        //       modified inside this loop.
        unsigned int endidx = NeighborPts.size();
        for (unsigned int idx = 0; idx < endidx; ++idx) {
          int neighbor_pt = NeighborPts[idx];
          if (!Visited[neighbor_pt]) {
            if (debug_ > 0) mprintf(" %i", neighbor_pt + 1);
            // Mark this neighbor as visited
            Visited[neighbor_pt] = true;
            // Determine how many other points are near this neighbor
            RegionQuery( Npts2, FramesToCluster, neighbor_pt );
            if ((int)Npts2.size() >= minPoints_) {
              // Add other points to current neighbor list
              NeighborPts.insert( NeighborPts.end(), Npts2.begin(), Npts2.end() );
              endidx = NeighborPts.size();
            }
          }
          // If neighbor is not yet part of a cluster, add it to this one.
          if (Status_[neighbor_pt] != INCLUSTER) {
            cluster_frames.push_back( neighbor_pt );
            Status_[neighbor_pt] = INCLUSTER;
          }
        }
        // Remove duplicate frames
        // TODO: Take care of this in Renumber?
        std::sort(cluster_frames.begin(), cluster_frames.end());
        ClusterDist::Cframes::iterator it = std::unique(cluster_frames.begin(), 
                                                        cluster_frames.end());
        cluster_frames.resize( std::distance(cluster_frames.begin(),it) );
        // Add cluster to the list
        AddCluster( cluster_frames );
        if (debug_ > 0) {
          mprintf("\n");
          PrintClusters();
        }
      }
    }
    cluster_progress.Update(iteration++);
  } // END loop over FramesToCluster
  // Calculate the distances between each cluster based on centroids
  ClusterDistances_.SetupMatrix( clusters_.size() );
  // Make sure centroid for clusters are up to date
  for (cluster_it C1 = clusters_.begin(); C1 != clusters_.end(); ++C1)
    (*C1).CalculateCentroid( Cdist_ );
  // Calculate distances between each cluster centroid
  cluster_it C1end = clusters_.end();
  for (cluster_it C1 = clusters_.begin(); C1 != C1end; ++C1) {
    cluster_it C2 = C1;
    ++C2;
    for (; C2 != clusters_.end(); ++C2)
      ClusterDistances_.AddElement( Cdist_->CentroidDist( (*C1).Cent(), (*C2).Cent() ) );
  }

  return 0;
}

// Cluster_DBSCAN::ClusterResults()
void Cluster_DBSCAN::ClusterResults(CpptrajFile& outfile) {
  // List the number of noise points.
  outfile.Printf("#NOISE_FRAMES:");
  unsigned int frame = 1;
  for (std::vector<char>::const_iterator stat = Status_.begin();
                                         stat != Status_.end(); 
                                       ++stat, ++frame)
  {
    if ( *stat == NOISE )
      outfile.Printf(" %i", frame);
  }
  outfile.Printf("\n");
}

// Cluster_DBSCAN::AddSievedFrames()
void Cluster_DBSCAN::AddSievedFrames() {
  int n_sieved_noise = 0;
  int Nsieved = 0;
  // NOTE: All cluster centroids must be up to date!
  if (sieveToCentroid_)
    mprintf("\tRestoring sieved frames by closeness to existing centroids.\n");
  else
    mprintf("\tRestoring sieved frames if within %.3f of frame in nearest cluster.\n",
            epsilon_);
  // Vars allocated here in case of OpenMP
  int frame, cidx;
  int nframes = (int)FrameDistances_.Nframes();
  double mindist, dist;
  cluster_it minNode, Cnode;
  bool goodFrame;
  ParallelProgress progress( nframes );
  // Need a temporary array to hold which frame belongs to which cluster. 
  // Otherwise we could be comparoing sieved frames to other sieved frames.
  std::vector<cluster_it> frameToCluster( nframes, clusters_.end() );
# ifdef _OPENMP
  int numthreads, mythread;
  // Need to create a ClusterDist for every thread to ensure memory allocation and avoid clashes
  ClusterDist** cdist_thread;
# pragma omp parallel
  {
    if (omp_get_thread_num()==0)
      numthreads = omp_get_num_threads();
  }
  mprintf("\tParallelizing calculation with %i threads\n", numthreads);
  cdist_thread = new ClusterDist*[ numthreads ];
  for (int i=0; i < numthreads; i++)
    cdist_thread[i] = Cdist_->Copy();
# pragma omp parallel private(mythread, frame, dist, mindist, minNode, Cnode, goodFrame, cidx) firstprivate(progress) reduction(+ : Nsieved, n_sieved_noise)
{
    mythread = omp_get_thread_num();
    progress.SetThread( mythread );
#   pragma omp for schedule(dynamic)
# endif
  for (frame = 0; frame < nframes; ++frame) {
    progress.Update( frame );
    if (FrameDistances_.IgnoringRow(frame)) {
      // Which clusters centroid is closest to this frame?
      mindist = DBL_MAX;
      minNode = clusters_.end();
      for (Cnode = clusters_.begin(); Cnode != clusters_.end(); ++Cnode) {
#       ifdef _OPENMP
        dist = cdist_thread[mythread]->FrameCentroidDist(frame, (*Cnode).Cent());
#       else
        dist = Cdist_->FrameCentroidDist(frame, (*Cnode).Cent());
#       endif
        if (dist < mindist) {
          mindist = dist;
          minNode = Cnode;
        }
      }
      goodFrame = false;
      if ( sieveToCentroid_ || mindist < epsilon_ )
        // Sieving based on centroid only or frame is already within epsilon, accept.
        goodFrame = true;
      else {
        // Check if any frames in the cluster are closer than epsilon to sieved frame.
        for (cidx=0; cidx < (*minNode).Nframes(); cidx++)
        {
#         ifdef _OPENMP
          if ( cdist_thread[mythread]->FrameDist(frame, (*minNode).ClusterFrame(cidx)) < epsilon_ )
#         else
          if ( Cdist_->FrameDist(frame, (*minNode).ClusterFrame(cidx)) < epsilon_ )
#         endif
          {
            goodFrame = true;
            break;
          }
        }
      }
      // Add sieved frame to the closest cluster if closest distance is
      // less than epsilon.
      ++Nsieved;
      if ( goodFrame )
        frameToCluster[frame] = minNode;
      else
        n_sieved_noise++;
    }
  } // END loop over frames
# ifdef _OPENMP
} // END pragma omp parallel
  // Free cdist_thread memory
  for (int i = 0; i < numthreads; i++)
    delete cdist_thread[i];
  delete[] cdist_thread;
# endif
  progress.Finish();
  // Now actually add sieved frames to their appropriate clusters
  for (frame = 0; frame < nframes; frame++)
    if (frameToCluster[frame] != clusters_.end())
      (*frameToCluster[frame]).AddFrameToCluster( frame );
  mprintf("\t%i of %i sieved frames were discarded as noise.\n", 
          n_sieved_noise, Nsieved);
}
