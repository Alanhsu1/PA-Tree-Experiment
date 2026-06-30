#include "FreeRTOS.h"
#include "driverlib.h"
#include <stdlib.h>

float voltage_value;

#define VOLTAGE_RECORD_MAX_COUNT 300

#pragma PERSISTENT(split_voltage_arr)
float split_voltage_arr[VOLTAGE_RECORD_MAX_COUNT] = {0};

#pragma PERSISTENT(split_voltage_arr_head)
uint16_t split_voltage_arr_head = 0;

#pragma PERSISTENT(compaction_voltage_arr)
float compaction_voltage_arr[VOLTAGE_RECORD_MAX_COUNT] = {0};

#pragma PERSISTENT(compaction_voltage_arr_head)
uint16_t compaction_voltage_arr_head = 0;

void init_adc()
{
  GPIO_setAsPeripheralModuleFunctionOutputPin(
      GPIO_PORT_P3,
      GPIO_PIN1,
      GPIO_TERNARY_MODULE_FUNCTION);
  while (Ref_A_isRefGenBusy(REF_A_BASE))
    ;

  // Select internal ref = 1.2V
  Ref_A_setReferenceVoltage(REF_A_BASE,
                            REF_A_VREF1_2V);

  // Turn on Reference Voltage
  Ref_A_enableReferenceVoltage(REF_A_BASE);

  ADC12_B_initParam initParam = {0};
  initParam.sampleHoldSignalSourceSelect = ADC12_B_SAMPLEHOLDSOURCE_SC;
  initParam.clockSourceSelect = ADC12_B_CLOCKSOURCE_ADC12OSC;
  initParam.clockSourceDivider = ADC12_B_CLOCKDIVIDER_1;
  initParam.clockSourcePredivider = ADC12_B_CLOCKPREDIVIDER__1;
  initParam.internalChannelMap = ADC12_B_NOINTCH;
  ADC12_B_init(ADC12_B_BASE, &initParam);

  // Enable the ADC12B module
  ADC12_B_enable(ADC12_B_BASE);

  /*
   * Base address of ADC12B Module
   * For memory buffers 0-7 sample/hold for 64 clock cycles
   * For memory buffers 8-15 sample/hold for 4 clock cycles (default)
   * Disable Multiple Sampling
   */
  ADC12_B_setupSamplingTimer(ADC12_B_BASE,
                             ADC12_B_CYCLEHOLD_16_CYCLES,
                             ADC12_B_CYCLEHOLD_4_CYCLES,
                             ADC12_B_MULTIPLESAMPLESDISABLE);

  // Configure Memory Buffer
  /*
   * Base address of the ADC12B Module
   * Configure memory buffer 0
   * Map input A1 to memory buffer 0
   * Vref+ = IntBuffer
   * Vref- = AVss
   * Memory buffer 0 is not the end of a sequence
   */
  ADC12_B_configureMemoryParam configureMemoryParam = {0};
  configureMemoryParam.memoryBufferControlIndex = ADC12_B_MEMORY_0;
  configureMemoryParam.inputSourceSelect = ADC12_B_INPUT_A13;
  configureMemoryParam.refVoltageSourceSelect = ADC12_B_VREFPOS_INTBUF_VREFNEG_VSS;
  configureMemoryParam.endOfSequence = ADC12_B_NOTENDOFSEQUENCE;
  configureMemoryParam.windowComparatorSelect = ADC12_B_WINDOW_COMPARATOR_DISABLE;
  configureMemoryParam.differentialModeSelect = ADC12_B_DIFFERENTIAL_MODE_DISABLE;
  ADC12_B_configureMemory(ADC12_B_BASE, &configureMemoryParam);

  __delay_cycles(75); // reference settling ~75us
}

static inline void enable_adc(){
    ADC12CTL0 |= ADC12ENC | ADC12SC;
}

static inline void disable_adc(){
    ADC12CTL0 &= ~(ADC12ENC | ADC12SC);
}

static float sample_voltage(float *record_arr, uint16_t *record_head)
{
  enable_adc();
  // voltage_value = (float)((ADC12MEM0) * (1.275 / 1024.0));
  voltage_value = (float)((ADC12MEM0) * (1.315 / 1024.0));
  // voltage_value = (float)((ADC12MEM0) * (1.295 / 1024.0));
  if (record_arr != NULL && record_head != NULL && *record_head < VOLTAGE_RECORD_MAX_COUNT)
  {
    record_arr[(*record_head)++] = voltage_value;
  }
  disable_adc();
  return voltage_value;
}

float get_voltage()
{
  return sample_voltage(NULL, NULL);
}

float get_split_voltage()
{
  return sample_voltage(split_voltage_arr, &split_voltage_arr_head);
}

float get_compaction_voltage()
{
  return sample_voltage(compaction_voltage_arr, &compaction_voltage_arr_head);
}

float peek_voltage()
{
  return sample_voltage(NULL, NULL);
}
