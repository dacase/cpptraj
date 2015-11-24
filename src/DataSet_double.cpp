#include "DataSet_double.h"
#ifdef MPI
# include "Parallel.h"
#endif

// DataSet_double::Allocate()
/** Reserve space in the Data and Frames arrays. */
int DataSet_double::Allocate( SizeArray const& sizeIn ) {
  if (!sizeIn.empty())
    Data_.reserve( sizeIn[0] );
  return 0;
}

// DataSet_double::Add()
/** Insert data vIn at frame. */
void DataSet_double::Add(size_t frame, const void* vIn) {
  if (frame > Data_.size())
    Data_.resize( frame, 0.0 );
  // Always insert at the end
  // NOTE: No check for duplicate frame values.
  Data_.push_back( *((double*)vIn) );
}

// DataSet_double::WriteBuffer()
/** Write data at frame to CharBuffer. If no data for frame write 0.0.
  */
void DataSet_double::WriteBuffer(CpptrajFile &cbuffer, SizeArray const& frame) const {
  if (frame[0] >= Data_.size())
    cbuffer.Printf(format_.fmt(), 0.0);
  else
    cbuffer.Printf(format_.fmt(), Data_[frame[0]]);
}

// DataSet_double::Append()
int DataSet_double::Append(DataSet* dsIn) {
  if (dsIn->Empty()) return 0;
  if (dsIn->Group() != SCALAR_1D) return 1;
  if (dsIn->Type() == DOUBLE) {
    size_t oldsize = Size();
    std::vector<double> const& dataIn = ((DataSet_double*)dsIn)->Data_;
    Data_.resize( oldsize + dataIn.size() );
    std::copy( dataIn.begin(), dataIn.end(), Data_.begin() + oldsize );
  } else {
    DataSet_1D const& ds = static_cast<DataSet_1D const&>( *dsIn );
    for (unsigned int i = 0; i != ds.Size(); i++)
      Data_.push_back( ds.Dval(i) );
  }
  return 0;
}

// DataSet_double::Sync()
int DataSet_double::Sync(size_t total, std::vector<int> const& rank_frames) {
# ifdef MPI
  if (Parallel::World().Size()==1) return 0;
  if (Parallel::World().Master()) {
    size_t pos = Data_.size();
    // Need to increase size of Data on master by number of frames on each other rank.
    int additional_frames = (int)total - rank_frames[0];
    Data_.resize( Data_.size() + additional_frames );
    double* endptr = &(Data_[0]) + pos;
    // Receive data from each rank.
    for (int rank = 1; rank < Parallel::World().Size(); rank++) {
      Parallel::World().SendMaster( endptr, rank_frames[rank], rank, MPI_DOUBLE );
      endptr += rank_frames[rank];
    }
  } else // Send data to master //TODO adjust for repeated additions?
    Parallel::World().SendMaster( &(Data_[0]), Data_.size(), Parallel::World().Rank(), MPI_DOUBLE );
# endif
  return 0;
}
