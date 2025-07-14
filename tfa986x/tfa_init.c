/*
 * Copyright (C) 2014-2020 NXP Semiconductors, All Rights Reserved.
 * Copyright 2020 GOODIX, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include "inc/dbgprint.h"
#include "inc/tfa_service.h"
#include "inc/tfa_internal.h"
#include "inc/tfa_container.h"
#include "inc/tfa98xx_tfafieldnames.h"

/* The CurrentSense4 registers are not in the datasheet */
#define TFA98XX_CURRENTSENSE4_CTRL_CLKGATECFOFF (1 << 2)
#define TFA98XX_CURRENTSENSE4 0x49

/*********************/
/* GLOBAL (Defaults) */
/*********************/

static bool ipc_loaded;

void tfa_set_ipc_loaded(int status)
{
	pr_info("set ipc_loaded %d for tfadsp\n", status);
	ipc_loaded = (status) ? true : false;
}

int tfa_get_ipc_loaded(void)
{
	pr_info("get ipc_loaded %d for tfadsp\n", ipc_loaded);
	return ipc_loaded ? 1 : 0;
}

static enum tfa98xx_error
	no_overload_function_available(struct tfa_device *tfa, int not_used)
{
	(void)tfa;
	(void)not_used;

	return TFA98XX_ERROR_OK;
}

static enum tfa98xx_error
	no_overload_function_available2(struct tfa_device *tfa)
{
	(void)tfa;

	return TFA98XX_ERROR_OK;
}

/* tfa98xx_dsp_system_stable
 *  return: *ready = 1 when clocks are stable to allow DSP subsystem access
 */
static enum tfa98xx_error tfa_dsp_system_stable(struct tfa_device *tfa,
	int *ready)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	unsigned short status;
	int value;

	/* check the contents of the STATUS register */
	value = TFA_READ_REG(tfa, AREFS);
	if (value < 0) {
		error = -value;
		*ready = 0;
		_ASSERT(error);		/* an error here can be fatal */
		return error;
	}
	status = (unsigned short)value;

	/* check AREFS and CLKS: not ready if either is clear */
	*ready = !((TFA_GET_BF_VALUE(tfa, AREFS, status) == 0)
		|| (TFA_GET_BF_VALUE(tfa, CLKS, status) == 0));

	return error;
}

/* tfa98xx_toggle_mtp_clock
 * Allows to stop clock for MTP/FAim needed for PLMA5505
 */
static enum tfa98xx_error tfa_faim_protect(struct tfa_device *tfa, int state)
{
	(void)tfa;
	(void)state;

	return TFA98XX_ERROR_OK;
}

/** Set internal oscillator into power down mode.
 *
 *  This function is a worker for tfa98xx_set_osc_powerdown().
 *
 *  @param[in] tfa device description structure
 *  @param[in] state new state 0 - oscillator is on, 1 oscillator is off.
 *
 *  @return TFA98XX_ERROR_OK when successful, error otherwise.
 */
static enum tfa98xx_error
	tfa_set_osc_powerdown(struct tfa_device *tfa, int state)
{
	/* This function has no effect in general case, only for tfa9912 */
	(void)tfa;
	(void)state;

	return TFA98XX_ERROR_OK;
}

static enum tfa98xx_error tfa_update_lpm(struct tfa_device *tfa, int state)
{
	/* This function has no effect in general case, only for tfa9912 */
	(void)tfa;
	(void)state;

	return TFA98XX_ERROR_OK;
}

static enum tfa98xx_error tfa_dsp_reset(struct tfa_device *tfa, int state)
{
	/* generic function */
	//TFA_SET_BF_VOLATILE(tfa, RST, (uint16_t)state);

	return TFA98XX_ERROR_OK;
}

static int tfa_set_swprofile(struct tfa_device *tfa, unsigned short new_value)
{
	int active_value = tfa->profile;

	/* Also set the new value in the struct */
	tfa->profile = new_value - 1;

	/* for TFA1 devices */
	/* it's in MTP shadow, so unlock if not done already */
	//mtpk = TFA_GET_BF(tfa, MTPK); /* get current key */
	//TFA_SET_BF_VOLATILE(tfa, MTPK, 0x5a);
	TFA_SET_BF_VOLATILE(tfa, SWPROFIL, new_value); /* set current profile */
	//TFA_SET_BF_VOLATILE(tfa, MTPK, (uint16_t)mtpk); /* restore key */

	return active_value;
}

static int tfa_get_swprofile(struct tfa_device *tfa)
{
	/* return TFA_GET_BF(tfa, SWPROFIL) - 1; */
	return tfa->profile;
}

static int tfa_set_swvstep(struct tfa_device *tfa, unsigned short new_value)
{
	int active_value = tfa->vstep;

	/* Also set the new value in the struct */
	tfa->vstep = new_value - 1;

	/* for TFA1 devices */
	/* it's in MTP shadow, so unlock if not done already */
	//mtpk = TFA_GET_BF(tfa, MTPK); /* get current key */
	//TFA_SET_BF_VOLATILE(tfa, MTPK, 0x5a);
	TFA_SET_BF_VOLATILE(tfa, SWVSTEP, new_value); /* set current vstep */
	//TFA_SET_BF_VOLATILE(tfa, MTPK, (uint16_t)mtpk); /* restore key */

	return active_value;
}

static int tfa_get_swvstep(struct tfa_device *tfa)
{
	int value = 0;
	/* Set the new value in the hw register */
	value = TFA_GET_BF(tfa, SWVSTEP);

	/* Also set the new value in the struct */
	tfa->vstep = value - 1;

	return value - 1; /* invalid if 0 */
}

static int tfa_get_mtpb(struct tfa_device *tfa)
{

	int value = 0;

	/* Set the new value in the hw register */
	//value = TFA_GET_BF(tfa, MTPB);

	return value;
}

static enum tfa98xx_error tfa_set_mute_nodsp(struct tfa_device *tfa, int mute)
{
	(void)tfa;
	(void)mute;

	return TFA98XX_ERROR_OK;
}

static int tfa_set_bitfield(struct tfa_device *tfa,
	uint16_t bitfield, uint16_t value)
{
	return tfa_set_bf(tfa, (uint16_t)bitfield, value);
}

static void set_ops_defaults(struct tfa_device_ops *ops)
{
	/* defaults */
	ops->reg_read = tfa98xx_read_register16;
	ops->reg_write = tfa98xx_write_register16;
	if (!ipc_loaded) {
		ops->dsp_msg = tfa_dsp_msg_rpc;
		ops->dsp_msg_read = tfa_dsp_msg_read_rpc;
	}
	ops->dsp_write_tables = no_overload_function_available;
	ops->dsp_reset = tfa_dsp_reset;
	ops->dsp_system_stable = tfa_dsp_system_stable;
	ops->auto_copy_mtp_to_iic = no_overload_function_available2;
	ops->factory_trimmer = no_overload_function_available2;
	ops->set_swprof = tfa_set_swprofile;
	ops->get_swprof = tfa_get_swprofile;
	ops->set_swvstep = tfa_set_swvstep;
	ops->get_swvstep = tfa_get_swvstep;
	ops->get_mtpb = tfa_get_mtpb;
	ops->set_mute = tfa_set_mute_nodsp;
	ops->faim_protect = tfa_faim_protect;
	ops->set_osc_powerdown = tfa_set_osc_powerdown;
	ops->update_lpm = tfa_update_lpm;
	ops->set_bitfield = tfa_set_bitfield;
}

/****************************/
/* no TFA
 * external DSP SB instance
 ****************************/
static short tfanone_swvstep, swprof; /* TODO emulate in hal plugin */
static enum tfa98xx_error tfanone_dsp_system_stable(struct tfa_device *tfa,
	int *ready)
{
	(void)tfa; /* suppress warning */
	*ready = 1; /* assume always ready */

	return TFA98XX_ERROR_OK;
}

static int tfanone_set_swprofile(struct tfa_device *tfa,
	unsigned short new_value)
{
	int active_value = tfa_dev_get_swprof(tfa);

	/* Set the new value in the struct */
	tfa->profile = new_value - 1;

	/* Set the new value in the hw register */
	swprof = new_value;

	return active_value;
}

static int tfanone_get_swprofile(struct tfa_device *tfa)
{
	(void)tfa; /* suppress warning */

	return swprof;
}

static int tfanone_set_swvstep(struct tfa_device *tfa,
	unsigned short new_value)
{
	/* Set the new value in the struct */
	tfa->vstep = new_value - 1;

	/* Set the new value in the hw register */
	tfanone_swvstep = new_value;

	return new_value;
}

static int tfanone_get_swvstep(struct tfa_device *tfa)
{
	(void)tfa; /* suppress warning */

	return tfanone_swvstep;
}

void tfanone_ops(struct tfa_device_ops *ops)
{
	/* Set defaults for ops */
	set_ops_defaults(ops);

	ops->dsp_system_stable = tfanone_dsp_system_stable;
	ops->set_swprof = tfanone_set_swprofile;
	ops->get_swprof = tfanone_get_swprofile;
	ops->set_swvstep = tfanone_set_swvstep;
	ops->get_swvstep = tfanone_get_swvstep;
}

/***********/
/* TFA986x */
/***********/
static enum tfa98xx_error tfa986x_specific(struct tfa_device *tfa)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	unsigned short value, xor, rc;
	unsigned short irqmask;
	int bf_value;

	if (tfa->in_use == 0)
		return TFA98XX_ERROR_NOT_OPEN;

	tfa_set_bf(tfa, TFA9866_BF_PWDN, 0);
	tfa_set_bf(tfa, TFA9866_BF_MANAOOSC, 0);

	rc = tfa_wait4manstate(tfa, TFA9866_BF_MANSTATE, 1, 50);
	if (rc < 0) {
		pr_err("Error, waiting powerdown leaving\n");
		return rc;
	}

	/* Unlock key 1 and 2 */
	error = reg_write(tfa, 0x0F, 0x5A6B);
	error = reg_read(tfa, 0xFB, &value);
	xor = value ^ 0x005A;
	error = reg_write(tfa, 0xA0, xor);
	tfa98xx_key2(tfa, 0);

	switch (tfa->revid) {
	case 0x1a66: /* Initial revision ID TFA9866 N1A1 */
		/* ----- generated code start ----- */
		/* -----  version 22 ----- */
		reg_write(tfa, 0x00, 0xf241); /* POR=0xf261 */
		reg_write(tfa, 0x02, 0x0628); /* POR=0x0008 */
		reg_write(tfa, 0x50, 0xc000); /* POR=0x8000 */
		reg_write(tfa, 0x5a, 0x5f4c); /* POR=0x36be */
		reg_write(tfa, 0x5b, 0x74e2); /* POR=0x7329 */
		reg_write(tfa, 0x5c, 0x302b); /* POR=0x5e96 */
		reg_write(tfa, 0x5f, 0x00a0); /* POR=0x00c0 */
		reg_write(tfa, 0x62, 0x05c6); /* POR=0x0582 */
		reg_write(tfa, 0x63, 0x80d4); /* POR=0x0602 */
		reg_write(tfa, 0x67, 0x0626); /* POR=0x0602 */
		reg_write(tfa, 0x68, 0x0820); /* POR=0x0c20 */
		reg_write(tfa, 0x74, 0x60f0); /* POR=0x4cf0 */
		reg_write(tfa, 0x75, 0x0e00); /* POR=0x1200 */
		reg_write(tfa, 0x78, 0x0001); /* POR=0x000d */
		reg_write(tfa, 0x7c, 0x10f2); /* POR=0x1602 */
		reg_write(tfa, 0xd7, 0x1000); /* POR=0x0000 */
		reg_write(tfa, 0xdd, 0x0036); /* POR=0x005e */
		/* ----- generated code end   ----- */
		break;

	case 0x2a66: /* Initial revision ID TFA9866 N1A2 */
		/* ----- generated code start ----- */
		/* -----  version 3 ----- */
		reg_write(tfa, 0x00, 0xf241); /* POR=0xf261 */
		reg_write(tfa, 0x02, 0x0628); /* POR=0x0008 */
		reg_write(tfa, 0x50, 0xc000); /* POR=0x8000 */
		reg_write(tfa, 0x5a, 0x5f4c); /* POR=0x36be */
		reg_write(tfa, 0x5b, 0x74e2); /* POR=0x7329 */
		reg_write(tfa, 0x5c, 0x302b); /* POR=0x5e96 */
		reg_write(tfa, 0x5f, 0x00a0); /* POR=0x00c0 */
		reg_write(tfa, 0x62, 0x05c6); /* POR=0x0582 */
		reg_write(tfa, 0x63, 0x80d4); /* POR=0x0602 */
		reg_write(tfa, 0x67, 0x0626); /* POR=0x0602 */
		reg_write(tfa, 0x68, 0x0820); /* POR=0x0c20 */
		reg_write(tfa, 0x74, 0x60f0); /* POR=0x4cf0 */
		reg_write(tfa, 0x75, 0x0e00); /* POR=0x1200 */
		reg_write(tfa, 0x78, 0x0001); /* POR=0x000d */
		reg_write(tfa, 0x7c, 0x10f2); /* POR=0x1602 */
		reg_write(tfa, 0xd7, 0x1000); /* POR=0x0000 */
		reg_write(tfa, 0xdd, 0x0036); /* POR=0x005e */
		/* ----- generated code end   ----- */
		break;

	case 0x3a66: /* Initial revision ID TFA9866 N1A3 */
		/* ----- generated code start ----- */
		/* -----  version 5 ----- */
		reg_write(tfa, 0x00, 0xf241); /* POR=0xf261 */
		reg_write(tfa, 0x02, 0x0628); /* POR=0x0008 */
		reg_write(tfa, 0x50, 0xc000); /* POR=0x8000 */
		reg_write(tfa, 0x5a, 0x5f4c); /* POR=0x36be */
		reg_write(tfa, 0x5b, 0x74e2); /* POR=0x7329 */
		reg_write(tfa, 0x5c, 0x302b); /* POR=0x5e96 */
		reg_write(tfa, 0x5f, 0x00a0); /* POR=0x00c0 */
		reg_write(tfa, 0x62, 0x05c4); /* POR=0x0582 */
		reg_write(tfa, 0x63, 0x80d4); /* POR=0x0602 */
		reg_write(tfa, 0x67, 0x0066); /* POR=0x0602 */
		reg_write(tfa, 0x68, 0x0820); /* POR=0x0c20 */
		reg_write(tfa, 0x74, 0x60f0); /* POR=0x4cf0 */
		reg_write(tfa, 0x75, 0x0e00); /* POR=0x1200 */
		reg_write(tfa, 0x78, 0x0001); /* POR=0x000d */
		reg_write(tfa, 0x7c, 0x10f2); /* POR=0x1602 */
		reg_write(tfa, 0xd7, 0x1000); /* POR=0x0000 */
		reg_write(tfa, 0xdd, 0x0036); /* POR=0x005e */
		/* ----- generated code end   ----- */
		break;

	case 0x100a66: /* Initial revision ID TFA9866 N2A0 */
		/* ----- generated code start ----- */
		/* -----  version 43 ----- */
		reg_write(tfa, 0x00, 0xf241); /* POR=0xf261 */
		reg_write(tfa, 0x02, 0x0c28); /* POR=0x0008 */
		reg_write(tfa, 0x08, 0x009a); /* POR=0x00d2 */
		reg_write(tfa, 0x50, 0xc000); /* POR=0x8000 */
		reg_write(tfa, 0x54, 0x20e0); /* POR=0x00e0 */
		reg_write(tfa, 0x5a, 0x5f5e); /* POR=0x36be */
		reg_write(tfa, 0x5b, 0x74e2); /* POR=0x7329 */
		reg_write(tfa, 0x5c, 0xb02b); /* POR=0xde96 */
		reg_write(tfa, 0x5f, 0x00a0); /* POR=0x00c0 */
		reg_write(tfa, 0x62, 0x06c4); /* POR=0x0582 */
		reg_write(tfa, 0x63, 0x80d4); /* POR=0x0602 */
		reg_write(tfa, 0x65, 0x0c58); /* POR=0x0458 */
		reg_write(tfa, 0x67, 0x006e); /* POR=0x0602 */
		reg_write(tfa, 0x68, 0x0820); /* POR=0x0c20 */
		reg_write(tfa, 0x69, 0x0119); /* POR=0x0319 */
		reg_write(tfa, 0x74, 0x6028); /* POR=0x4c14 */
		reg_write(tfa, 0x75, 0x1daa); /* POR=0x49e0 */
		reg_write(tfa, 0x78, 0x0001); /* POR=0x000d */
		reg_write(tfa, 0x7c, 0x10f2); /* POR=0x1602 */
		reg_write(tfa, 0xdd, 0x01b6); /* POR=0x01de */
		/* ----- generated code end   ----- */
		break;

	case 0x100b66:/**TFA9866 N2B0**/
		/* ----- generated code start(V8)----- */
		/* -----  version 10 ----- */
		reg_write(tfa, 0x00, 0xf241); //POR=0xf261
		reg_write(tfa, 0x02, 0x0c28); //POR=0x0008
		reg_write(tfa, 0x08, 0x009a); //POR=0x00d2
		reg_write(tfa, 0x50, 0xc000); //POR=0x8000
		reg_write(tfa, 0x54, 0x10e0); //POR=0x00e0
		reg_write(tfa, 0x5a, 0x5f5e); //POR=0x36be
		reg_write(tfa, 0x5b, 0x74e2); //POR=0x7329
		reg_write(tfa, 0x5c, 0xb02b); //POR=0xde96
		reg_write(tfa, 0x5f, 0x00a0); //POR=0x00c0
		reg_write(tfa, 0x62, 0x06c4); //POR=0x0682
		reg_write(tfa, 0x63, 0x80d4); //POR=0x0602
		reg_write(tfa, 0x65, 0x0c58); //POR=0x0458
		reg_write(tfa, 0x67, 0x006e); //POR=0x0602
		reg_write(tfa, 0x68, 0x0820); //POR=0x0c20
		reg_write(tfa, 0x69, 0x0119); //POR=0x0319
		reg_write(tfa, 0x74, 0x6028); //POR=0x4c14
		reg_write(tfa, 0x75, 0x1daa); //POR=0x49e0
		reg_write(tfa, 0x78, 0x0001); //POR=0x000d
		reg_write(tfa, 0x7c, 0x10f2); //POR=0x1602
		reg_write(tfa, 0xdd, 0x01b6); //POR=0x01de
		/* ----- generated code end   ----- */
		break;

	case 0x200a66:/**TFA9866 N3A0**/		
		bf_value = tfa_get_bf(tfa, TFA9866_BF_SPARE_F0_15_10);
		if (bf_value >= 0)
			tfa_set_bf(tfa, TFA9866_BF_CS_KTEMP, (uint16_t)bf_value);

		/* ----- generated code start(V8)----- */
		/* -----  version 12 ----- */
		reg_write(tfa, 0x08, 0x009a); //POR=0x00d2
		reg_write(tfa, 0x50, 0xc000); //POR=0x8000
		reg_write(tfa, 0x62, 0x0666); //POR=0x06c6
		reg_write(tfa, 0x63, 0x806d); //POR=0x80d4
		reg_write(tfa, 0x65, 0x0c58); //POR=0x0458
		reg_write(tfa, 0x67, 0x016d); //POR=0x0628
		reg_write(tfa, 0x74, 0x5e28); //POR=0x6014
		reg_write(tfa, 0x75, 0x1daa); //POR=0x39e0
		/* ----- generated code end   ----- */
		break;

	case 0x201a66:/**TFA9866 N3A1**/		
		bf_value = tfa_get_bf(tfa, TFA9866_BF_SPARE_F0_15_10);
		if (bf_value >= 0)
			tfa_set_bf(tfa, TFA9866_BF_CS_KTEMP, (uint16_t)bf_value);

		/* ----- generated code start(V8)----- */
		/* -----  version 6 ----- */
		reg_write(tfa, 0x00, 0xf201); //POR=0xf241
		reg_write(tfa, 0x08, 0x009a); //POR=0x00d2
		reg_write(tfa, 0x50, 0xc000); //POR=0x8000
		reg_write(tfa, 0x54, 0x40e0); //POR=0x00e0
		reg_write(tfa, 0x62, 0x0666); //POR=0x06c6
		reg_write(tfa, 0x63, 0x806d); //POR=0x80d4
		reg_write(tfa, 0x65, 0x0c58); //POR=0x0458
		reg_write(tfa, 0x67, 0x016d); //POR=0x0628
		reg_write(tfa, 0x74, 0x5e28); //POR=0x6014
		reg_write(tfa, 0x75, 0x1daa); //POR=0x39e0
		/* ----- generated code end   ----- */
		break;

	default:
		pr_info("\nWarning: Optimal settings not found for device with revid = 0x%x\n",
			tfa->revid);
		break;
	}

	/* select interrupt flags */
	irqmask = (TFA_BF_MSK(TFA9866_BF_IEOTDS)
		| TFA_BF_MSK(TFA9866_BF_IEOCPR)
		| TFA_BF_MSK(TFA9866_BF_IEUVDS)
		| TFA_BF_MSK(TFA9866_BF_IEOVDS)
		| TFA_BF_MSK(TFA9866_BF_IEDCTH)
		| TFA_BF_MSK(TFA9866_BF_IEBODNOK));
	tfa->interrupt_enable[0] = irqmask; /* save mask */
	tfa_irq_init(tfa); /* init irq regs */

	tfa_set_bf(tfa, TFA9866_BF_PWDN, 1); /* 1 = off */
	rc = tfa_wait4manstate(tfa, TFA9866_BF_MANSTATE, 0, 50);
	if (rc < 0) {
		pr_err("Timeout waiting for manstate 0\n");
		return rc;
	}
	/* we come from reset state so turn off osc */
	tfa_set_bf(tfa, TFA9866_BF_MANAOOSC, 1);

	return error;
}

static int tfa986x_set_swprofile(struct tfa_device *tfa,
	unsigned short new_value)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	int active_value = tfa_dev_get_swprof(tfa);

	/* Set the new value in the struct */
	tfa->profile = new_value - 1;

	if (tfa->swprof != tfa->profile)
		/* Set the new value in the hw register */
		err = tfa_set_bf_volatile(tfa,
			TFA9866_BF_SWPROFIL, new_value);

	if (err == TFA98XX_ERROR_OK)
		tfa->swprof = tfa->profile;

	return active_value;
}

static int tfa986x_get_swprofile(struct tfa_device *tfa)
{
	if (tfa->swprof == -1)
		tfa->swprof = tfa_get_bf(tfa, TFA9866_BF_SWPROFIL) - 1;

	return tfa->swprof;
}

static int tfa986x_set_swvstep(struct tfa_device *tfa,
	unsigned short new_value)
{
	/* Set the new value in the struct */
	tfa->vstep = new_value - 1;

	return new_value;
}

static int tfa986x_get_swvstep(struct tfa_device *tfa)
{
	return tfa->vstep; /* vstep is not used */
}

/* tfa98xx_dsp_system_stable
 *  return: *ready = 1 when clocks are stable to allow DSP subsystem access
 */
static enum tfa98xx_error tfa986x_dsp_system_stable(struct tfa_device *tfa,
	int *ready)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;

	/* check CLKS: ready if set */
	*ready = tfa_get_bf(tfa, TFA9866_BF_CLKS) == 1;

	return error;
}

static int tfa986x_set_bitfield(struct tfa_device *tfa,
	uint16_t bitfield, uint16_t value)
{
	uint16_t reg = (bitfield >> 8) & 0xff;

	if (reg == TFA98XX_STATUS_FLAGS0
		|| reg == TFA98XX_STATUS_FLAGS3)
		return tfa_set_bf_volatile(tfa, (uint16_t)bitfield, value);
	else
		return tfa_set_bf(tfa, (uint16_t)bitfield, value);
}

void tfa986x_ops(struct tfa_device_ops *ops)
{
	/* Set defaults for ops */
	set_ops_defaults(ops);

	ops->get_mtpb = NULL; /* no mtp */
	ops->tfa_init = tfa986x_specific;
	ops->set_swprof = tfa986x_set_swprofile;
	ops->get_swprof = tfa986x_get_swprofile;
	ops->set_swvstep = tfa986x_set_swvstep;
	ops->get_swvstep = tfa986x_get_swvstep;
	ops->dsp_system_stable = tfa986x_dsp_system_stable;
	ops->set_mute = tfa_set_mute_nodsp;
	ops->set_bitfield = tfa986x_set_bitfield;
}

