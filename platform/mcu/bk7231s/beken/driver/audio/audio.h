#ifndef _BK_AUDIO_H_
#define _BK_AUDIO_H_

#define AUDIO_BASE                                   (0x00802B00)

#define AUDIO_CONFIG                                 (AUDIO_BASE + 0x0 * 4)
#define SAMPLE_RATE_ADC_POSI                         (0)
#define SAMPLE_RATE_ADC_MASK                         (0x3U)
#define SAMPLE_RATE_8K                               (0)
#define SAMPLE_RATE_16K                              (1)
#define SAMPLE_RATE_44_1_K                           (2)
#define SAMPLE_RATE_48K                              (3)
#define DAC_ENABLE                                   (1 << 2)
#define ADC_ENABLE                                   (1 << 3)
#define DTMF_ENABLE                                  (1 << 4)
#define LINEIN_ENABLE                                (1 << 5)
#define SAMPLE_RATE_DAC_POSI                         (6)
#define SAMPLE_RATE_DAC_MASK                         (0x3U)

#define AUD_DTMF_CONFIG_0                            (AUDIO_BASE + 0x1 * 4)
#define TONE_PATTERN                                 (1 << 0)
#define TONE_MODE                                    (1 << 0)
#define TONE_PAUSE_TIME_POSI                         (2)
#define TONE_PAUSE_TIME_MASK                         (0xFU)
#define TONE_ACTIVE_TIME_POSI                        (6)
#define TONE_ACTIVE_TIME_MASK                        (0xFU)

#define AUD_DTMF_CONFIG_1                            (AUDIO_BASE + 0x2 * 4)
#define TONE_1_STEP_POSI                             (0)
#define TONE_1_STEP_MASK                             (0xFFFFU)
#define TONE_1_ATTU_POSI                             (16)
#define TONE_1_ATTU_MASK                             (0xFU)
#define TONE_1_ENABLE                                (1 << 20)

#define AUD_DTMF_CONFIG_2                            (AUDIO_BASE + 0x3 * 4)
#define TONE_2_STEP_POSI                             (0)
#define TONE_2_STEP_MASK                             (0xFFFFU)
#define TONE_2_ATTU_POSI                             (16)
#define TONE_2_ATTU_MASK                             (0xFU)
#define TONE_2_ENABLE                                (1 << 20)

#define AUD_ADC_CONFIG_0                             (AUDIO_BASE + 0x4 * 4)
#define ADC_HPF2_COEF_B2_POSI                        (0)
#define ADC_HPF2_COEF_B2_MASK                        (0xFFFFU)
#define ADC_HPF2_BYPASS                              (1 << 16)
#define ADC_HPF1_BYPASS                              (1 << 17)
#define ADC_SET_GAIN_POSI                            (18)
#define ADC_SET_GAIN_MASK                            (0x3FU)
#define ADC_SAMPLE_EDGE                              (1 << 24)

#define AUD_ADC_CONFIG_1                             (AUDIO_BASE + 0x5 * 4)
#define ADC_HPF2_COEF_B0_POSI                        (0)
#define ADC_HPF2_COEF_B0_MASK                        (0xFFFFU)
#define ADC_HPF2_COEF_B1_POSI                        (16)
#define ADC_HPF2_COEF_B1_MASK                        (0xFFFFU)

#define AUD_ADC_CONFIG_2                             (AUDIO_BASE + 0x6 * 4)
#define ADC_HPF2_COEF_A0_POSI                        (0)
#define ADC_HPF2_COEF_A0_MASK                        (0xFFFFU)
#define ADC_HPF2_COEF_A1_POSI                        (16)
#define ADC_HPF2_COEF_A1_MASK                        (0xFFFFU)

#define AUD_DAC_CONFIG_0                             (AUDIO_BASE + 0x7 * 4)
#define DAC_HPF2_COEF_B2_POSI                        (0)
#define DAC_HPF2_COEF_B2_MASK                        (0xFFFFU)
#define DAC_HPF2_BYPASS                              (1 << 16)
#define DAC_HPF1_BYPASS                              (1 << 17)
#define DAC_SET_GAIN_POSI                            (18)
#define DAC_SET_GAIN_MASK                            (0x3FU)
#define DAC_CLK_INVERT                               (1 << 24)

#define AUD_DAC_CONFIG_1                             (AUDIO_BASE + 0x8 * 4)
#define DAC_HPF2_COEF_B0_POSI                        (0)
#define DAC_HPF2_COEF_B0_MASK                        (0xFFFFU)
#define DAC_HPF2_COEF_B1_POSI                        (16)
#define DAC_HPF2_COEF_B1_MASK                        (0xFFFFU)

#define AUD_DAC_CONFIG_2                             (AUDIO_BASE + 0x9 * 4)
#define DAC_HPF2_COEF_A1_POSI                        (0)
#define DAC_HPF2_COEF_A1_MASK                        (0xFFFFU)
#define DAC_HPF2_COEF_A2_POSI                        (16)
#define DAC_HPF2_COEF_A2_MASK                        (0xFFFFU)

#define AUD_FIFO_CONFIG                              (AUDIO_BASE + 0xA * 4)
#define DAC_R_RD_THRED_POSI                          (0)
#define DAC_R_RD_THRED_MASK                          (0x1FU)
#define DAC_L_RD_THRED_POSI                          (5)
#define DAC_L_RD_THRED_MASK                          (0x1FU)
#define DTMF_WR_THRED_POSI                           (10)
#define DTMF_WR_THRED_MASK                           (0x1FU)
#define ADC_WR_THRED_POSI                            (15)
#define ADC_WR_THRED_MASK                            (0x1FU)
#define DAC_R_INT_EN                                 (1 << 20)
#define DAC_L_INT_EN                                 (1 << 21)
#define DTMF_INT_EN                                  (1 << 22)
#define ADC_INT_EN                                   (1 << 23)
#define LOOP_TON2DAC                                 (1 << 24)
#define LOOP_ADC2DAC                                 (1 << 25)

#define AUD_AGC_CONFIG_0                             (AUDIO_BASE + 0xB * 4)
#define AGC_NOISE_THRED_POSI                         (0)
#define AGC_NOISE_THRED_MASK                         (0x3FFU)
#define AGC_NOISE_HIGH_POSI                          (10)
#define AGC_NOISE_HIGH_MASK                          (0x3FFU)
#define AGC_NOISE_LOW_POSI                           (20)
#define AGC_NOISE_LOW_MASK                           (0x3FFU)
#define AGC_STEP_POSI                                (30)
#define AGC_STEP_MASK                                (0x3U)

#define AUD_AGC_CONFIG_1                             (AUDIO_BASE + 0xC * 4)
#define AGC_NOISE_MIN_POSI                           (0)
#define AGC_NOISE_MIN_MASK                           (0x7FU)
#define AGC_NOISE_TOUT_POSI                          (7)
#define AGC_NOISE_TOUT_MASK                          (0x7U)
#define AGC_HIGH_DUR_POSI                            (10)
#define AGC_HIGH_DUR_MASK                            (0x7U)
#define AGC_LOW_DUR_POSI                             (13)
#define AGC_LOW_DUR_MASK                             (0x7U)
#define AGC_MIN_POSI                                 (16)
#define AGC_MIN_MASK                                 (0x7FU)
#define AGC_MAX_POSI                                 (23)
#define AGC_MAX_MASK                                 (0x7FU)
#define AGC_NG_METHOD                                (1 << 30)
#define AGC_NG_ENABLE                                (1 << 31)

#define AUD_AGC_CONFIG_2                             (AUDIO_BASE + 0xD * 4)
#define AGC_DECAY_TIME_POSI                          (0)
#define AGC_DECAY_TIME_MASK                          (0x7U)
#define AGC_ATTACK_TIME_POSI                         (3)
#define AGC_ATTACK_TIME_MASK                         (0x7U)
#define AGC_HIGH_THRD_POSI                           (6)
#define AGC_HIGH_THRD_MASK                           (0x1FU)
#define AGC_LOW_THRED_POSI                           (11)
#define AGC_LOW_THRED_MASK                           (0x1FU)
#define AGC_IIR_COEF_POSI                            (16)
#define AGC_IIR_COEF_MAS                             (0x7U)
#define AGC_ENABLE                                   (1 << 19)
#define MANUAL_PGA_VAL_POSI                          (20)
#define MANUAL_PGA_VAL_MASK                          (0x7FU)
#define MANUAL_PGA_MODE                              (1 << 27)

#define AUD_AD_FIFO_STATUS                           (AUDIO_BASE + 0xE * 4)
#define DAC_R_NEAR_FULL                              (1 << 0)
#define DAC_L_NEAR_FULL                              (1 << 1)
#define ADC_NEAR_FULL                                (1 << 2)
#define DTMF_NEAR_FULL                               (1 << 3)
#define DAC_R_NEAR_EMPTY                             (1 << 4)
#define DAC_L_NEAR_EMPTY                             (1 << 5)
#define ADC_NEAR_EMPTY                               (1 << 6)
#define DTMF_NEAR_EMPTY                              (1 << 7)
#define DAC_R_FIFO_FULL                              (1 << 8)
#define DAC_L_FIFO_FULL                              (1 << 9)
#define ADC_FIFO_FULL                                (1 << 10)
#define DTMF_FIFO_FULL                               (1 << 11)
#define DAC_R_FIFO_EMPTY                             (1 << 12)
#define DAC_L_FIFO_EMPTY                             (1 << 13)
#define ADC_FIFO_EMPTY                               (1 << 14)
#define DTMF_FIFO_EMPTY                              (1 << 15)
#define DAC_R_INT_FLAG                               (1 << 16)
#define DAC_L_INT_FLAG                               (1 << 17)
#define ADC_INT_FLAG                                 (1 << 18)
#define DTMF_INT_FLAG                                (1 << 19)

#define AUD_AGC_STATUS                               (AUDIO_BASE + 0xF * 4)
#define AGC_RSSI_POSI                                (0)
#define AGC_RSSI_MASK                                (0xFFU)
#define AGC_MIC_PGA_POSI                             (8)
#define AGC_MIC_PGA_MASK                             (0xFFU)
#define AGC_MIC_RSSI_POSI                            (16)
#define AGC_MIC_RSSI_MASK                            (0xFFFFU)

#define AUD_DTMF_FIFO_PORT                           (AUDIO_BASE + 0x10 * 4)
#define AD_DTMF_FIFO_MASK                            (0xFFFFU)

#define AUD_ADC_FIFO_PORT                            (AUDIO_BASE + 0x11 * 4)
#define AD_ADC_L_FIFO_MASK                           (0xFFFFU)
#define AD_ADC_L_FIFO_POSI                           (0)
#define AD_ADC_R_FIFO_MASK                           (0xFFFFU)
#define AD_ADC_R_FIFO_POSI                           (16)
#define AD_ADC_LR_FIFO_MASK                          (0xFFFFFFFFU)

#define AUD_DAC_FIFO_PORT                            (AUDIO_BASE + 0x12 * 4)
#define AD_DAC_L_FIFO_POSI                           (0)
#define AD_DAC_L_FIFO_MASK                           (0xFFFFU)
#define AD_DAC_R_FIFO_POSI                           (16)
#define AD_DAC_R_FIFO_MASK                           (0xFFFFU)

#define AUD_EXTEND_CFG                               (AUDIO_BASE + 0x18 * 4)
#define DAC_FRACMOD_MANUAL                           (1 << 0)
#define ADC_FRACMOD_MANUAL                           (1 << 1)
#define FILT_ENABLE                                  (1 << 2)

#define AUD_DAC_FRACMOD                              (AUDIO_BASE + 0x19 * 4)

#define AUD_ADC_FRACMOD                              (AUDIO_BASE + 0x1A * 4)

#define AUD_HPF2_EXT_COEF                            (AUDIO_BASE + 0x1F * 4)
#define HPF2_A1_L_6BIT_POSI                          (0)
#define HPF2_A1_L_6BIT_MASK                          (0x3FU)
#define HPF2_A2_L_6BIT_POSI                          (6)
#define HPF2_A2_L_6BIT_MASK                          (0x3FU)
#define HPF2_B0_L_6BIT_POSI                          (12)
#define HPF2_B0_L_6BIT_MASK                          (0x3FU)
#define HPF2_B1_L_6BIT_POSI                          (18)
#define HPF2_B1_L_6BIT_MASK                          (0x3FU)
#define HPF2_B2_L_6BIT_POSI                          (24)
#define HPF2_B2_L_6BIT_MASK                          (0x3FU)

#define AUD_FLT_0_COEF_1                             (AUDIO_BASE + 0x20 * 4)
#define FLT_0_A1_POSI                                (0)
#define FLT_0_A1_MASK                                (0xFFFFU)
#define FLT_0_A2_POSI                                (16)
#define FLT_0_A2_MASK                                (0xFFFFU)
#define AD_FLT_0_COEF_2                              (AUDIO_BASE + 0x21 * 4)
#define FLT_0_B0_POSI                                (0)
#define FLT_0_B0_MASK                                (0xFFFFU)
#define FLT_0_B1_POSI                                (16)
#define FLT_0_B1_MASK                                (0xFFFFU)
#define AD_FLT_0_COEF_3                              (AUDIO_BASE + 0x22 * 4)
#define FLT_0_B2_POSI                                (0)
#define FLT_0_B2_MASK                                (0xFFFFU)

#define AUD_FLT_1_COEF_1                             (AUDIO_BASE + 0x23 * 4)
#define FLT_1_A1_POSI                                (0)
#define FLT_1_A1_MASK                                (0xFFFFU)
#define FLT_1_A2_POSI                                (16)
#define FLT_1_A2_MASK                                (0xFFFFU)
#define AD_FLT_1_COEF_2                              (AUDIO_BASE + 0x24 * 4)
#define FLT_1_B0_POSI                                (0)
#define FLT_1_B0_MASK                                (0xFFFFU)
#define FLT_1_B1_POSI                                (16)
#define FLT_1_B1_MASK                                (0xFFFFU)
#define AD_FLT_1_COEF_3                              (AUDIO_BASE + 0x25 * 4)
#define FLT_1_B2_POSI                                (0)
#define FLT_1_B2_MASK                                (0xFFFFU)

#define AUD_FLT_2_COEF_1                             (AUDIO_BASE + 0x26 * 4)
#define FLT_2_A1_POSI                                (0)
#define FLT_2_A1_MASK                                (0xFFFFU)
#define FLT_2_A2_POSI                                (16)
#define FLT_2_A2_MASK                                (0xFFFFU)
#define AD_FLT_2_COEF_2                              (AUDIO_BASE + 0x27 * 4)
#define FLT_2_B0_POSI                                (0)
#define FLT_2_B0_MASK                                (0xFFFFU)
#define FLT_2_B1_POSI                                (16)
#define FLT_2_B1_MASK                                (0xFFFFU)
#define AD_FLT_2_COEF_3                              (AUDIO_BASE + 0x28 * 4)
#define FLT_2_B2_POSI                                (0)
#define FLT_2_B2_MASK                                (0xFFFFU)

#define AUD_FLT_3_COEF_1                             (AUDIO_BASE + 0x29 * 4)
#define FLT_3_A1_POSI                                (0)
#define FLT_3_A1_MASK                                (0xFFFFU)
#define FLT_3_A2_POSI                                (16)
#define FLT_3_A2_MASK                                (0xFFFFU)
#define AD_FLT_3_COEF_2                              (AUDIO_BASE + 0x2A * 4)
#define FLT_3_B0_POSI                                (0)
#define FLT_3_B0_MASK                                (0xFFFFU)
#define FLT_3_B1_POSI                                (16)
#define FLT_3_B1_MASK                                (0xFFFFU)
#define AD_FLT_3_COEF_3                              (AUDIO_BASE + 0x2B * 4)
#define FLT_3_B2_POSI                                (0)
#define FLT_3_B2_MASK                                (0xFFFFU)

#define AUD_FLT_0_EXT_COEF                           (AUDIO_BASE + 0x2C * 4)
#define FLT_A1_L_6BIT_POSI                           (0)
#define FLT_A1_L_6BIT_MASK                           (0x3FU)
#define FLT_A2_L_6BIT_POSI                           (6)
#define FLT_A2_L_6BIT_MASK                           (0x3FU)
#define FLT_B0_L_6BIT_POSI                           (12)
#define FLT_B0_L_6BIT_MASK                           (0x3FU)
#define FLT_B1_L_6BIT_POSI                           (18)
#define FLT_B1_L_6BIT_MASK                           (0x3FU)
#define FLT_B2_L_6BIT_POSI                           (24)
#define FLT_B2_L_6BIT_MASK                           (0x3FU)

#define AUD_FLT_1_EXT_COEF                           (AUDIO_BASE + 0x2D * 4)

#define AUD_FLT_2_EXT_COEF                           (AUDIO_BASE + 0x2E * 4)

#define AUD_FLT_3_EXT_COEF                           (AUDIO_BASE + 0x2F * 4)

#define CONST_DIV_441K               (0x049B2368)
#define CONST_DIV_48K                (0x043B5554)
#define CONST_DIV_16K                (0x06590000)
#define CONST_DIV_32K                (0x01964000)

extern void audio_power_up(void);
extern void audio_power_down(void);
extern void audio_enable_interrupt(void);
extern void audio_disable_interrupt(void);
extern void audio_dac_software_init(void);
extern void audio_dac_isr(void);

extern void audio_adc_enable_linein(void);
extern void audio_adc_disable_linein(void);
extern void audio_adc_software_init(void);
extern void audio_adc_isr(void);

#include "uart_pub.h"
#define AUD_DEBUG                    (1)
#if AUD_DEBUG
#define AUD_PRT                      os_printf
#else
#define AUD_PRT                      null_prf
#define AUD_WPRT                     null_prf
#endif

#endif // _BK_AUDIO_H_
