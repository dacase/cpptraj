#include <cstring>
#include <cstdlib> // for abs, malloc, free
#include "Action_DSSP.h"
#include "CpptrajStdio.h"
// Dssp

// CONSTRUCTOR
DSSP::DSSP() {
  outfilename=NULL;
  dssp=NULL; 
  Nres=0;
  Nframe=0.0;
  sumOut=NULL;
  SSline=NULL;
  printString=true;
  SSdata=NULL;
  dsspData=NULL;
}

// DESTRUCTOR
DSSP::~DSSP() {
//  debugout.CloseFile(); // DEBUG
  if (sumOut!=NULL) free(sumOut);
  if (SSline!=NULL) free(SSline);
  if (SSdata!=NULL) delete(SSdata);
  if (dsspData!=NULL) delete(dsspData);
}

const char DSSP::SSchar[7]={ '0', 'b', 'B', 'G', 'H', 'I', 'T' };
const char DSSP::SSname[7][6]={"None", "Para", "Anti", "3-10", "Alpha", "Pi", "Turn"};

/* DSSP::init()
 * Expected call: secstruct [out <filename>] [<mask>] [sumout <filename>]
 * If sumout is not specified the filename specified by out is used with .sum suffix. 
 * Arg. check order is:
 *    1) Keywords
 *    2) Masks
 * For now dont allow NULL(stdout) filename for output
 */
int DSSP::init() {
  char *mask, *temp;

  // DEBUG
//  debugout.SetupFile((char*)"dsspdebug.out",WRITE,UNKNOWN_FORMAT,STANDARD,0);
//  debugout.OpenFile();

  // Get keywords
  outfilename = A->getKeyString("out",NULL);
  temp = A->getKeyString("sumout",NULL);
  if (temp!=NULL) {
    sumOut = (char*) malloc( (strlen(temp) + 1) * sizeof(char));
    strcpy(sumOut, temp);
  } else if (outfilename!=NULL) {
    sumOut = (char*) malloc( ( strlen(outfilename) + 5) * sizeof(char));
    strcpy(sumOut, outfilename);
    strcat(sumOut,".sum");
  } 
  if (A->hasKey("nostring")) printString=false;
  // Get masks
  mask = A->getNextMask();
  Mask.SetMaskString(mask);

  // Set up the DSSP data set
  if (printString) {
    dssp = DSL->Add(STRING, A->getNextString(),"DSSP");
    if (dssp==NULL) return 1;
    DFL->Add(outfilename, dssp);
  }

  mprintf( "    SECSTRUCT: Calculating secondary structure using mask [%s]\n",Mask.maskString);
  if (outfilename!=NULL) 
    mprintf("               Dumping results to %s\n", outfilename);
  if (sumOut!=NULL)
    mprintf("               Sum results to %s\n",sumOut);
  if (printString) 
    mprintf("               SS data for each residue will be stored as a string.\n");
  else
    mprintf("               SS data for each residue will be stored as integers.\n");

  return 0;
}

/* DSSP::setup()
 * NOTE: Currently relatively  memory-intensive. Eventually set up so that SecStruct and
 * CO_HN_Hbond members exist only for selected residues. Use Map?
 */
int DSSP::setup() {
  int selected, atom, res;
  Residue RES;
  // For Integer dataset, store residue name + number. 32 should be more than
  // adequate (4 char + 27 int + 1 null).
  char resArg[32];

  // Set up mask for this parm
  if ( Mask.SetupMask(P,debug) ) return 1;
  if ( Mask.None() ) {
    mprintf("      Error: DSSP::setup: Mask has no atoms.\n");
    return 1;
  }

  // Set up SecStruct for each solute residue
  if (P->finalSoluteRes>0)
    Nres = P->finalSoluteRes;
  else
    Nres=P->nres;
  //mprintf("      DSSP: Setting up for %i residues.\n",Nres);

  // Set up for each residue of the current Parm if not already set-up.
  for (res = 0; res < Nres; res++) {
    if (res>=(int)SecStruct.size()) {
      RES.sstype=SECSTRUCT_NULL;
      RES.isSelected=false;
      RES.C=-1;
      RES.O=-1;
      RES.N=-1;
      RES.H=-1;
      RES.CO_HN_Hbond.assign( Nres, 0 );
      RES.SSprob[0]=0.0;
      RES.SSprob[1]=0.0;
      RES.SSprob[2]=0.0;
      RES.SSprob[3]=0.0;
      RES.SSprob[4]=0.0;
      RES.SSprob[5]=0.0;
      RES.SSprob[6]=0.0;
      SecStruct.push_back(RES);
    }
  }

  // Go through all atoms in mask. Set up a residue for each C, O, N, and H atom
  for (selected=0; selected < Mask.Nselected; selected++) {
    atom = Mask.Selected[selected];
    res = P->atomToResidue(atom);
    //fprintf(stdout,"DEBUG: Atom %i Res %i [%s]\n",atom,res,P->names[atom]);
    SecStruct[res].isSelected=true;
    if (      strcmp(P->names[atom], "C   ")==0 )
      SecStruct[res].C=atom;
    else if ( strcmp(P->names[atom], "O   ")==0 )
      SecStruct[res].O=atom;
    else if ( strcmp(P->names[atom], "N   ")==0 )
      SecStruct[res].N=atom;
    else if ( strcmp(P->names[atom], "H   ")==0 )
      SecStruct[res].H=atom;
  }

  // Count number of selected residues
  selected=0;
  for (res=0; res < Nres; res++)
    if (SecStruct[res].isSelected) selected++;
  mprintf("      DSSP: [%s] corresponds to %i residues.\n",Mask.maskString,selected);

  // Make an integer dataset to hold SS type/frame for each residue.
  if (!printString) {
    if (SSdata==NULL) SSdata = new DataSetList();
    for (res=0; res < Nres; res++) {
      if (SecStruct[res].isSelected) {
        // Setup dataset name for this residue
        P->ResName(resArg,res);
        // Create dataset for res - if already present this returns NULL
        DataSet *resDataSet = SSdata->AddIdx(INT, resArg, res);
        if (resDataSet!=NULL) DFL->Add(outfilename, resDataSet);
      }
    }
  // Otherwise set up output buffer to hold string
  } else {
    SSline = (char*) realloc(SSline, (selected+1) * sizeof(char));
    SSline[selected]='\0';
  }

  // DEBUG - Print atom nums for each residue set up
//  for (res=0; res < Nres; res++) {
//    if (SecStruct[res].isSelected)
//      fprintf(stdout,"DEBUG: %i C=%i O=%i N=%i H=%i\n",res,SecStruct[res].C,
//              SecStruct[res].O, SecStruct[res].N, SecStruct[res].H);
//  }

  return 0;
}

/* DSSP::isBonded()
 * Return 1 if residue 1 CO bonded to residue 2 NH.
 * Ensure residue numbers are valid and residues are selected.
 */
int DSSP::isBonded(int res1, int res2) {
  if (res1<0) return 0;
  if (res2<0) return 0;
  if (res1>=Nres) return 0;
  if (res2>=Nres) return 0;
  if (!SecStruct[res1].isSelected) return 0;
  if (!SecStruct[res2].isSelected) return 0;
  if ( SecStruct[res1].CO_HN_Hbond[res2] ) return 1;
  return 0;
}

/* DSSP::SSassign()
 * Assign all residues from res1 to res2-1 the secondary structure sstype
 * only if not already assigned.
 * Assumes given residue range is valid.
 */
void DSSP::SSassign(int res1, int res2, SStype typeIn) {
  for (int res=res1; res<res2; res++) {
    if (res==Nres) break;
    if (!SecStruct[res].isSelected) continue;
    if (SecStruct[res].sstype==SECSTRUCT_NULL)
      SecStruct[res].sstype=typeIn;
  }
}   
 
/* DSSP::action()
 * Determine secondary structure by hydrogen bonding pattern.
 */    
int DSSP::action() {
  int resi, resj;
  int C, O, H, N;
  double rON, rCH, rOH, rCN, E;

  // Determine C=0 to H-N hydrogen bonds for each residue to each other residue
#ifdef _OPENMP
#pragma omp parallel private(resi,resj,C,O,H,N,rON, rCH, rOH, rCN, E)
{
#pragma omp for
#endif
  for (resi=0; resi < Nres; resi++) {
    if (!SecStruct[resi].isSelected) continue;
    // Reset previous SS assignment
    SecStruct[resi].sstype=SECSTRUCT_NULL;
    SecStruct[resi].CO_HN_Hbond.assign( Nres, 0 );    

    if (SecStruct[resi].C==-1 || SecStruct[resi].O==-1) continue;
    C = SecStruct[resi].C;
    O = SecStruct[resi].O;
    for (resj=0; resj < Nres; resj++) {
// DEBUG
//      debugout.IO->Printf("\n%i Res%i-Res%i:",currentFrame,resi,resj);
//      debugout.IO->Printf(" C=%i O=%i | N=%i H=%i:",C,O,SecStruct[resj].N,SecStruct[resj].H);
// DEBUG 
      if (!SecStruct[resj].isSelected) continue;
      if (resi==resj) continue;
      // NOTE: Should check all atoms here?
      if (SecStruct[resj].H==-1 || SecStruct[resj].N==-1) continue;
      
      N = SecStruct[resj].N;
      H = SecStruct[resj].H;

      rON = F->DIST(O, N);
      rCH = F->DIST(C, H);
      rOH = F->DIST(O, H);
      rCN = F->DIST(C, N);

      E = DSSP_fac * (1/rON + 1/rCH - 1/rOH - 1/rCN);
      if (E < -0.5)
        SecStruct[resi].CO_HN_Hbond[resj] = 1;
//      if ( SecStruct[resi].CO_HN_Hbond[resj] ) debugout.IO->Printf(" HBONDED!"); // DEBUG
    }
  }
#ifdef _OPENMP
} // END pragma omp parallel
#endif

  // Determine Secondary Structure based on Hbonding pattern.
  // In case of structural overlap, priority is given to the structure first in this list: 
  //   H, B, (E), G, I, T  (s. p. 2595 in the Kabsch & Sander paper)
  for (resi=0; resi < Nres; resi++) {
    if (!SecStruct[resi].isSelected) continue;

    // Alpha helices
    if ( isBonded( resi - 1, resi+3 ) && isBonded( resi, resi + 4) ) {
      SSassign(resi,resi+4,SECSTRUCT_ALPHA);
      continue;
    }

    // Beta sheets - only needed if SS not already assigned
    if ( SecStruct[resi].sstype == SECSTRUCT_NULL ) {
      for (resj=0; resj < Nres; resj++) {
        if (!SecStruct[resj].isSelected) continue;
        // Only consider residues spaced more than 2 apart
        if (abs(resi - resj) > 2) {
          // Parallel
          if ( (isBonded(resi-1, resj) && isBonded(resj, resi+1)) ||
               (isBonded(resj-1, resi) && isBonded(resi, resj+1)) ) {
            SecStruct[resi].sstype = SECSTRUCT_PARA;
            break;
          // Anti-parallel
          } else if ( (isBonded(resi-1, resj+1) && isBonded(resj-1, resi+1)) ||
                      (isBonded(resi,   resj  ) && isBonded(resj,   resi  )) ) {
            SecStruct[resi].sstype = SECSTRUCT_ANTI;
            break;
          }
        }
      }
      if (SecStruct[resi].sstype!=SECSTRUCT_NULL) continue; 
    }

    // 3-10 helix
    if ( isBonded( resi - 1, resi+2 ) && isBonded( resi, resi + 3) ) {
      SSassign(resi,resi+3,SECSTRUCT_3_10);
      continue;
    } 

    // Pi helix
    if ( isBonded( resi - 1, resi+4 ) && isBonded( resi, resi + 5) ) {
      SSassign(resi,resi+5,SECSTRUCT_PI);
      continue;
    }
  } // End Initial SS assignment over all residues

  // Assign Turn structure
  for (resi=0; resi < Nres; resi++) {
    if (!SecStruct[resi].isSelected) continue;

    for (resj=5; resj > 2; resj--) {
//      fprintf(stdout,"DEBUG: %i Res %i and %i+%i are",currentFrame,resi,resi,resj);
//      if ( isBonded( resi, resi+resj) )
//        fprintf(stdout," BONDED!\n");
//      else
//        fprintf(stdout," not bonded.\n");
      if ( isBonded( resi, resi+resj) ) {
        SSassign(resi+1, resi+resj, SECSTRUCT_TURN);
        break;
      }
    }
  }

  // Store data 
  //fprintf(stdout,"%10i ",currentFrame);
  resj=0;
  if (SSdata!=NULL) SSdata->Begin();
  for (resi=0; resi < Nres; resi++) {
    if (!SecStruct[resi].isSelected) continue;
    //fprintf(stdout,"%c",SSchar[SecStruct[resi].sstype]);
    SecStruct[resi].SSprob[SecStruct[resi].sstype]++;
    // Integer data set
    if (SSdata!=NULL) 
      SSdata->AddData(currentFrame, &(SecStruct[resi].sstype));
    else
      SSline[resj++] = SSchar[SecStruct[resi].sstype];
  }
  if (printString)
    dssp->Add(currentFrame, SSline);
  //fprintf(stdout,"\n");
  Nframe++;

  return 0;
}

/* DSSP::print()
 * Calculate the average of each secondary structure type across all residues
 * and output to a file.
 */
void DSSP::print() {
  DataFile *dsspFile;
  int resi, ss;
  double avg;

  // If not printing a string, sync the integer dataset here since
  // it is not part of the master dataset list.
  if (!printString && SSdata!=NULL) SSdata->Sync();

  if (sumOut==NULL) return;
  // Set up dataset list to store averages
  dsspData = new DataSetList(); 
  // Set up a dataset for each SS type
  for (ss=1; ss<7; ss++) 
    dsspFile = DFL->Add(sumOut, dsspData->Add(DOUBLE, (char*)SSname[ss], "SS") );
  // Change the X label to Residue
  dsspFile->SetXlabel((char*)"Residue");
  // Dont print empty frames
  dsspFile->SetNoEmptyFrames();

  // Calc the avg structure of each type for each selected residue 
  for (resi=0; resi < Nres; resi++) {
    if (!SecStruct[resi].isSelected) continue;
    dsspData->Begin();
    for (ss=1; ss<7; ss++) {
      avg = SecStruct[resi].SSprob[ss] / Nframe;
      dsspData->AddData(resi, &avg);
    }
  }
}
