/*
 * DSP Module - KISS FFT library binding + other DSP tools
 *
 * Copyright (C) 2006-2007 David Olofson
 */

#ifndef EEL_DSP_H
#define EEL_DSP_H

#include "EEL.h"

/*
 * Generator
 */
typedef struct
{
	;
} EDSP_generator;
EEL_MAKE_CAST(EDSP_generator)

EEL_xno eel_dsp_init(EEL_vm *vm);

#endif /* EEL_DSP_H */
