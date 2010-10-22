#if   defined(PRECISION_z)
#include "generated/zgemm_NN.h"
#include "generated/zgemm_NT.h"
#include "generated/zgemm_TN.h"
#include "generated/zgemm_TT.h"
#elif defined(PRECISION_c)
#include "generated/cgemm_NN.h"
#include "generated/cgemm_NT.h"
#include "generated/cgemm_TN.h"
#include "generated/cgemm_TT.h"
#elif defined(PRECISION_d)
#include "generated/dgemm_NN.h"
#include "generated/dgemm_NT.h"
#include "generated/dgemm_TN.h"
#include "generated/dgemm_TT.h"
#elif defined(PRECISION_s)
#include "generated/sgemm_NN.h"
#include "generated/sgemm_NT.h"
#include "generated/sgemm_TN.h"
#include "generated/sgemm_TT.h"
#endif