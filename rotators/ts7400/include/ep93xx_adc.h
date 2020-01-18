#define ADC_PAGE          0x80900000
#define ADCRESULT_OFFSET  0x0008
#define SDR_MASK          0x80000000
#define DATA_OFFSET       0x0008
#define DATA_MASK         0xFFFF
#define ADCSWITCH_OFFSET  0x0018
#define ADC_CH0           0x0608
#define ADC_CH1           0x0680
#define ADC_CH2           0x0640
#define ADC_CH3           0x0620
#define ADC_CH4           0x0610
#define ADCSWLOCK_OFFSET  0x0020
#define UNLOCK_VAL        0xAA

#define SYSCON_PAGE       0x80930000
#define ADCCLKDIV_OFFSET  0x0090
#define SYSCON_UNLOCK     0x00C0
#define TSEN_MASK         0x80000000
#define DEVICECFG_OFFSET  0x0080
#define ADCPD_MASK        0x04
#define ADCEN_MASK        0x20000

/*  prototypes  */
void init_ADC(unsigned long adc_page, unsigned long syscon_page);
int read_channel(unsigned long adc_page, unsigned short channel);
static char is_ADC_busy(unsigned long adc_page);

void init_ADC(unsigned long adc_page, unsigned long syscon_page)
{
	unsigned long val;

	/*    set TSEN bit     */
	val = PEEK32(syscon_page + ADCCLKDIV_OFFSET);
	//unlock the software lock
	POKE32(syscon_page + SYSCON_UNLOCK, UNLOCK_VAL);
	POKE32(syscon_page + ADCCLKDIV_OFFSET, TSEN_MASK | val);

	/*    set ADCEN bit    */
	val = PEEK32(syscon_page + DEVICECFG_OFFSET);
	POKE32(syscon_page + SYSCON_UNLOCK, UNLOCK_VAL); //unlock the soft lock
	POKE32(syscon_page + DEVICECFG_OFFSET, val | ADCEN_MASK);

	/*    clear ADCPD bit  */
	val = PEEK32(syscon_page + DEVICECFG_OFFSET);
	POKE32(adc_page + SYSCON_UNLOCK, UNLOCK_VAL); //unlock the soft lock
	POKE32(syscon_page + DEVICECFG_OFFSET, val & ~ADCPD_MASK);
}

int read_channel(unsigned long adc_page, unsigned short channel)
{
	unsigned long val;

	POKE32(adc_page + ADCSWLOCK_OFFSET, UNLOCK_VAL); //unlock the soft lock

	//write ADCSwitch reg to select channel
	POKE32(adc_page + ADCSWITCH_OFFSET, channel);

	while(is_ADC_busy(adc_page)); //poll ADCResult

	//read result from data regisyyter
	val = PEEK32(adc_page + DATA_OFFSET) ;

	val = val & DATA_MASK;

	return val;
}

static char is_ADC_busy(unsigned long adc_page)
{
	unsigned long val;

	val = PEEK32(adc_page + ADCRESULT_OFFSET);

	if((val & SDR_MASK) == SDR_MASK)
		return TRUE;

	return FALSE;
}
