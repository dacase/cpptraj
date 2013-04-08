#ifndef INC_DATASET_MATRIXDBL_H
#define INC_DATASET_MATRIXDBL_H
#include <vector>
#include "DataSet_2D.h"
#include "Matrix.h"
/// Double-precision two-dimensional matrix.
/** This is the class used by Action_Matrix. */
class DataSet_MatrixDbl : public DataSet_2D {
  public:
    DataSet_MatrixDbl() : DataSet_2D(MATRIX_DBL, 12, 4) {}
    double& operator[](size_t idx)             { return mat_[idx];          }
    static DataSet* Alloc() { return (DataSet*)new DataSet_MatrixDbl();     }
    // ----- DataSet functions -------------------
    size_t Size()                        const { return mat_.size();        }
    int Sync()                                 { return 1;                  }
    void Info()                          const { return;                    }
    // ----- DataSet_2D functions ----------------
    int Allocate2D(size_t x,size_t y)          { return mat_.resize(x,y);   }
    int AllocateHalf(size_t x)                 { return mat_.resize(x,0L);  }
    int AllocateTriangle(size_t x)             { return mat_.resize(0L,x);  }
    void Write2D(CpptrajFile&, int, int) const;
    double GetElement(size_t x,size_t y) const { return mat_.element(x,y);  }
    size_t Nrows()                       const { return mat_.Nrows();       }
    size_t Ncols()                       const { return mat_.Ncols();       }
    double* MatrixArray()                const;
    DataSet_2D::MType Kind()             const { return (DataSet_2D::MType)mat_.Type(); }
    DataSet_2D::MatrixType Type()        const { return type_;              }
    // -------------------------------------------
    int AddElement(double d)                   { return mat_.addElement(d); }
    void SetElement(size_t x,size_t y,double d){ mat_.setElement(x,y,d);    }
    // Iterator over matrix elements.
    typedef Matrix<double>::iterator iterator;
    iterator begin()                           { return mat_.begin();       }
    iterator end()                             { return mat_.end();         }
    // Iterator over diagonal vector elements.
    typedef std::vector<double> Darray;
    typedef Darray::iterator v_iterator;
    v_iterator v1begin()                       { return vect_.begin();      }
    v_iterator v1end()                         { return vect_.end();        }
    Darray::const_iterator v1begin()     const { return vect_.begin();      }
    Darray::const_iterator v1end()       const { return vect_.end();        }
    void AllocateVector(size_t vsize)          { vect_.resize(vsize, 0.0);  }
    size_t VectSize()                    const { return vect_.size();       }
    /// Set matrix type.
    void SetType(MatrixType tIn)               { type_ = tIn;               }
    void StoreMass(Darray const& mIn)          { mass_ = mIn;               }
    Darray const& Mass()                 const { return mass_;              }
    Darray const& Vect()                 const { return vect_;              }
  private:
    Matrix<double> mat_;       ///< Matrix elements.
    Darray vect_;              ///< Hold diagonal elements | avg coords
    Darray mass_;              ///< Hold masses, for MWCOVAR quasiharmonic analysis. 
    MatrixType type_;          ///< Matrix type.
};
#endif
