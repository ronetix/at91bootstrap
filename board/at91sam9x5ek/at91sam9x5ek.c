/* ----------------------------------------------------------------------------
 *         ATMEL Microcontroller Software Support  -  ROUSSET  -
 * ----------------------------------------------------------------------------
 * Copyright (c) 2008, Atmel Corporation

 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the disclaimer below.
 *
 * Atmel's name may not be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * DISCLAIMER: THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * ----------------------------------------------------------------------------
 * File Name           : at91sam9x5ek.c
 * Object              :
 * Creation            : HXu Aug. 2010
 *-----------------------------------------------------------------------------
 */
#if defined(WINCE) && !defined(CONFIG_AT91SAM9X5EK)

#else

#include "part.h"
#include "gpio.h"
#include "pmc.h"
#include "rstc.h"
#include "dbgu.h"
#include "debug.h"
#include "main.h"
#include "ddramc.h"

#ifdef CONFIG_NANDFLASH
#include "nandflash.h"
#endif

int get_cp15(void);

void set_cp15(unsigned int value);

int get_cpsr(void);

void set_cpsr(unsigned int value);

#ifdef CONFIG_HW_INIT
/*----------------------------------------------------------------------------*/
/* \fn    hw_init							      */
/* \brief This function performs very low level HW initialization	      */
/* This function is invoked as soon as possible during the c_startup	      */
/* The bss segment must be initialized					      */
/*----------------------------------------------------------------------------*/
void hw_init(void)
{
    unsigned int cp15;

    /*
     * Configure PIOs 
     */
    const struct pio_desc hw_pio[] = {
#ifdef CONFIG_DEBUG
        {"RXD", AT91C_PIN_PA(9), 0, PIO_DEFAULT, PIO_PERIPH_A},
        {"TXD", AT91C_PIN_PA(10), 0, PIO_DEFAULT, PIO_PERIPH_A},
#endif
        {(char *)0, 0, 0, PIO_DEFAULT, PIO_PERIPH_A},
    };

    /*
     * Disable watchdog 
     */
    writel(AT91C_WDTC_WDDIS, AT91C_BASE_WDTC + WDTC_WDMR);

    /*
     * At this stage the main oscillator is supposed to be enabled
     * * PCK = MCK = MOSC 
     */
    writel(0x00, AT91C_BASE_PMC + PMC_PLLICPR);

    /*
     * Configure PLLA = MOSC * (PLL_MULA + 1) / PLL_DIVA 
     */
    pmc_cfg_plla(PLLA_SETTINGS, PLL_LOCK_TIMEOUT);

    /*
     * PCK = PLLA/2 = 3 * MCK 
     */
    pmc_cfg_mck(BOARD_PRESCALER_MAIN_CLOCK, PLL_LOCK_TIMEOUT);

    /*
     * Switch MCK on PLLA output 
     */
    pmc_cfg_mck(BOARD_PRESCALER_PLLA, PLL_LOCK_TIMEOUT);

    /*
     * Enable External Reset 
     */
    writel(AT91C_RSTC_KEY_UNLOCK
           || AT91C_RSTC_URSTEN, AT91C_BASE_RSTC + RSTC_RMR);

    /*
     * Configure CP15 
     */
    cp15 = get_cp15();
    cp15 |= I_CACHE;
    set_cp15(cp15);

    /*
     * Configure the PIO controller 
     */
    writel((1 << AT91C_ID_PIOA_B), (PMC_PCER + AT91C_BASE_PMC));
    pio_setup(hw_pio);

    /*
     * Enable Debug messages on the DBGU 
     */
#ifdef CONFIG_DEBUG
    dbgu_init(BAUDRATE(MASTER_CLOCK, 115200));
    dbgu_print("Start AT91Bootstrap...\n\r");
#endif

#ifdef CONFIG_DDR2
    /*
     * Configure DDRAM Controller 
     */
    dbgu_print("Init DDR ...");
    ddramc_hw_init();
    dbgu_print("Done!\n\r");
#endif                          /* CONFIG_DDR2 */
}
#endif                          /* CONFIG_HW_INIT */

#ifdef CONFIG_DDR2
static SDdramConfig ddram_config;

/*------------------------------------------------------------------------------*/
/* \fn    ddramc_hw_init							*/
/* \brief This function performs DDRAMC HW initialization			*/
/*------------------------------------------------------------------------------*/
void ddramc_hw_init(void)
{
    ddram_config.ddramc_mdr =
        (AT91C_DDRC2_DBW_16_BITS | AT91C_DDRC2_MD_DDR2_SDRAM);

    ddram_config.ddramc_cr = (AT91C_DDRC2_NC_DDR10_SDR9 |       // 10 column bits (1K)
                              AT91C_DDRC2_NR_13 |       // 13 row bits    (8K)
                              AT91C_DDRC2_CAS_3 |       // CAS Latency 3
                              (1 << 20) | AT91C_DDRC2_DLL_RESET_DISABLED);      // DLL not reset

    ddram_config.ddramc_rtr = 0x411;    /* Refresh timer: 7.8125us */

    ddram_config.ddramc_t0pr = (AT91C_DDRC2_TRAS_6 |    //  6 * 7.5 = 45   ns
                                AT91C_DDRC2_TRCD_2 |    //  2 * 7.5 = 22.5 ns
                                AT91C_DDRC2_TWR_2 |     //  2 * 7.5 = 15   ns
                                AT91C_DDRC2_TRC_8 |     //  8 * 7.5 = 75   ns
                                AT91C_DDRC2_TRP_2 |     //  2 * 7.5 = 22.5 ns
                                AT91C_DDRC2_TRRD_2 |    //  2 * 7.5 = 15   ns
                                AT91C_DDRC2_TWTR_1 |    //  1 clock cycle
                                AT91C_DDRC2_TMRD_2);    //  2 clock cycles

    ddram_config.ddramc_t1pr = (AT91C_DDRC2_TXP_2 |     //  2 * 7.5 = 15 ns
                                200 << 16 |     // 200 clock cycles, TXSRD: Exit self refresh delay to Read command
                                19 << 8 |       // 16 * 7.5 = 120 ns TXSNR: Exit self refresh delay to non read command
                                AT91C_DDRC2_TRFC_18 << 0);      // 14 * 7.5 = 142 ns (must be 140 ns for 1Gb DDR)

    ddram_config.ddramc_t2pr = (AT91C_DDRC2_TRTP_1 |    //  1 * 7.5 = 7.5 ns
                                AT91C_DDRC2_TRPA_3 |    //  0 * 7.5 = 0 ns
                                AT91C_DDRC2_TXARDS_7 |  //  7 clock cycles
                                AT91C_DDRC2_TXARD_2);   //  2 clock cycles

    // ENABLE DDR2 clock 
    writel(AT91C_PMC_DDR, AT91C_BASE_PMC + PMC_SCER);

    /*
     * Chip select 1 is for DDR2/SDRAM
     */
    writel((readl(AT91C_BASE_CCFG + CCFG_EBICSA) | AT91C_EBI_CS1A_SDRAMC),
            AT91C_BASE_CCFG + CCFG_EBICSA);


    /*
     * DDRAM2 Controller
     */
    ddram_init(AT91C_BASE_DDR2C, AT91C_EBI_CS1, &ddram_config);
}
#endif                          /* CONFIG_DDR2 */

#ifdef CONFIG_NANDFLASH
/*------------------------------------------------------------------------------*/
/* \fn    nand_recovery						*/
/* \brief This function erases NandFlash Block 0 if BP4 is pressed 		*/
/*        during boot sequence							*/
/*------------------------------------------------------------------------------*/
static void nand_recovery(void)
{
}

/*------------------------------------------------------------------------------*/
/* \fn    nandflash_hw_init							*/
/* \brief NandFlash HW init							*/
/*------------------------------------------------------------------------------*/
void nandflash_hw_init(void)
{
    /*
     * Configure PIOs 
     */
    const struct pio_desc nand_pio[] = {
        {"NANDOE", AT91C_PIN_PD(0), 0, PIO_PULLUP, PIO_PERIPH_A},
        {"NANDWE", AT91C_PIN_PD(1), 0, PIO_PULLUP, PIO_PERIPH_A},
        {"NANDALE", AT91C_PIN_PD(2), 0, PIO_PULLUP, PIO_PERIPH_A},
        {"NANDCLE", AT91C_PIN_PD(3), 0, PIO_PULLUP, PIO_PERIPH_A},
        {"NANDCS", AT91C_PIN_PD(4), 0, PIO_PULLUP, PIO_OUTPUT},
        {"RDY_BSY", AT91C_PIN_PD(6), 0, PIO_PULLUP, PIO_INPUT}, //REVISIT
        {(char *)0, 0, 0, PIO_DEFAULT, PIO_PERIPH_A},
    };

    /*
     * Setup Nand flash logic
     */
    writel((readl(AT91C_BASE_CCFG + CCFG_EBICSA) | AT91C_EBI_CS3A_SM),
            AT91C_BASE_CCFG + CCFG_EBICSA);

    /*
     * Setup Nand flash data bus configuration (D0-D15 or D16-D31)
     * Note: for EK early kits choose D0-15
     */
    writel((readl(AT91C_BASE_CCFG + CCFG_EBICSA) & ~AT91C_EBI_NFD0_ON_D16),
            AT91C_BASE_CCFG + CCFG_EBICSA);

    /*
     * EBI IO drive configuration
     */
    writel(readl(AT91C_BASE_CCFG + CCFG_EBICSA) & ~AT91C_EBI_DRV,
           AT91C_BASE_CCFG + CCFG_EBICSA);

    /*
     * Configure SMC CS3 
     */
    writel((AT91C_SM_NWE_SETUP | AT91C_SM_NCS_WR_SETUP | AT91C_SM_NRD_SETUP |
            AT91C_SM_NCS_RD_SETUP), AT91C_BASE_SMC + SMC_SETUP3);
    writel((AT91C_SM_NWE_PULSE | AT91C_SM_NCS_WR_PULSE | AT91C_SM_NRD_PULSE |
            AT91C_SM_NCS_RD_PULSE), AT91C_BASE_SMC + SMC_PULSE3);
    writel((AT91C_SM_NWE_CYCLE | AT91C_SM_NRD_CYCLE),
           AT91C_BASE_SMC + SMC_CYCLE3);
    writel((AT91C_SMC_READMODE | AT91C_SMC_WRITEMODE |
            AT91C_SMC_NWAITM_NWAIT_DISABLE | AT91C_SMC_DBW_WIDTH_EIGTH_BITS |
            AT91C_SM_TDF), AT91C_BASE_SMC + SMC_CTRL3);

    /*
     * Configure the PIO controller 
     */
    writel((1 << AT91C_ID_PIOC_D), (PMC_PCER + AT91C_BASE_PMC));
    pio_setup(nand_pio);

    nand_recovery();
}

/*------------------------------------------------------------------------------*/
/* \fn    nandflash_cfg_16bits_dbw_init						*/
/* \brief Configure SMC in 16 bits mode						*/
/*------------------------------------------------------------------------------*/
void nandflash_cfg_16bits_dbw_init(void)
{
    writel(readl(AT91C_BASE_SMC + SMC_CTRL3) | AT91C_SMC_DBW_WIDTH_SIXTEEN_BITS,
           AT91C_BASE_SMC + SMC_CTRL3);
}

/*------------------------------------------------------------------------------*/
/* \fn    nandflash_cfg_8bits_dbw_init						*/
/* \brief Configure SMC in 8 bits mode						*/
/*------------------------------------------------------------------------------*/
void nandflash_cfg_8bits_dbw_init(void)
{
    writel((readl(AT91C_BASE_SMC + SMC_CTRL3) & ~(AT91C_SMC_DBW)) |
           AT91C_SMC_DBW_WIDTH_EIGTH_BITS, AT91C_BASE_SMC + SMC_CTRL3);
}

#endif                          /* #ifdef CONFIG_NANDFLASH */

void one_wire_hw_init(void)
{
	const struct pio_desc wire_pio[] = {
		{"1-Wire", AT91C_PIN_PB(18), 0, PIO_DEFAULT, PIO_OUTPUT},
		{(char *)0, 0, 0, PIO_DEFAULT, PIO_PERIPH_A},
	};

	writel((1 << AT91C_ID_PIOA_B), (PMC_PCER + AT91C_BASE_PMC));
	pio_setup(wire_pio);
}

#ifdef CONFIG_SCLK
void sclk_enable(void)
{
    writel(readl(AT91C_SYS_SLCKSEL) | AT91C_SLCKSEL_OSC32EN, AT91C_SYS_SLCKSEL);
    /* must wait for slow clock startup time ~ 1000ms
     * (~6 core cycles per iteration, core is at 400MHz: 66666000 min loops) */
    Wait(66700000);

    writel(readl(AT91C_SYS_SLCKSEL) | AT91C_SLCKSEL_OSCSEL, AT91C_SYS_SLCKSEL);
    /* must wait 5 slow clock cycles = ~153 us
     * (~6 core cycles per iteration, core is at 400MHz: min 10200 loops) */
    Wait(10200);

    /* now disable the internal RC oscillator */
    writel(readl(AT91C_SYS_SLCKSEL) & ~AT91C_SLCKSEL_RCEN, AT91C_SYS_SLCKSEL);
}
#endif                          /* CONFIG_SCLK */

#endif                          /* CONFIG_AT91SAM9G45EK */
