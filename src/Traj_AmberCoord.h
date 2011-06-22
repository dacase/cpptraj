#ifndef INC_TRAJ_AMBERCOORD_H
#define INC_TRAJ_AMBERCOORD_H
#include "TrajectoryIO.h"
/// Class: AmberCoord
/// TrajectoryIO class for reading and writing formatted (ASCII text) amber 
/// trajectories. 
/// NOTE: Use size_t and off_t for anything involving file calcs to avoid
/// implicit type conversions?
class AmberCoord: public TrajectoryIO {
    int natom3;         // Number of coords (# atoms X 3)
    int titleSize;      // Title size in bytes
    int frameSize;      // Coord frame size in bytes, inc box coords/REMD header if present
    int hasREMD;        // REMD header size if present
    //size_t frameSize; // Use size_t because this will be used with fread?
    char *frameBuffer;  // Hold 1 coord frame (inc. box coords)
    int numBoxCoords;   // Number of box coords, 3 (ortho or truncoct) or 6 (triclinic)

    // Inherited functions
    int setupRead(int);
    int setupWrite(int);
    int openTraj();
    void closeTraj();
    int readFrame(int,double*,double*,double*);
    int writeFrame(int,double*,double*,double*);
    void info();

  public:
    AmberCoord();
    ~AmberCoord();
    // AmberCoord-specific functions
    void SetRemdTraj();
};
#endif
