#ifndef MATRIXRUN_H
#define MATRIXRUN_H

#include <stdint.h>

void MatrixRun_Init(void);
void MatrixRun_Run(void);
void MatrixRun_SetCongestionBulk(const uint8_t data[8]);
void MatrixRun_SetScreen(uint8_t screen);
void MatrixRun_SetAutoCycle(uint8_t enable);
void MatrixRun_SetTrainDest(uint8_t dest_code);
uint8_t MatrixRun_GetTrainDest(void);

#endif /* MATRIXRUN_H */
