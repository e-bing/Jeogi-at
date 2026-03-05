#ifndef MATRIXRUN_H
#define MATRIXRUN_H

#include <stdint.h>

void MatrixRun_Init(void);
void MatrixRun_Run(void);
void MatrixRun_SetCongestionBulk(const uint8_t data[8]);

#endif /* MATRIXRUN_H */
