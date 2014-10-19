/*
---------------------------------------------------------------------------
	ec_optimizer.h - EEL Bytecode Optimizer
---------------------------------------------------------------------------
 * Copyright 2014 David Olofson
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from the
 * use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#ifndef	EEL_EC_OPTIMIZER_H
#define	EEL_EC_OPTIMIZER_H

#include "e_eel.h"

typedef enum EEL_optimizerflags
{
	/*
	 * Disable substitutions that would eliminate register
	 * initializations. (The optimizer isn't clever enough to analyze the
	 * full fragment + register allocations for dependencies, so we need
	 * to give it some hints for now.)
	 */
	EEL_OPTIMIZE_KEEP_REGISTERS =		0x00000001
} EEL_optimizerflags;

void eel_optimize(EEL_coder *cdr, unsigned flags);

#endif	/* EEL_EC_OPTIMIZER_H */
