#include "string.h"
#include "nrf_esb.h"
#include "app_common.h"

#define DATA_STORE_PAGE			127

void ds_get(uint32_t *p, uint16_t length){

	uint16_t i;
	if(length % 4) return;
	
	uint32_t page_size = NRF_FICR->CODEPAGESIZE;
	uint32_t *addr = (uint32_t *)(page_size * DATA_STORE_PAGE);
	
	for(i=0; i<length; i+=sizeof(uint32_t)){
		
		*(p++) = *(addr++);
	}
}

void ds_update(uint32_t *p, uint16_t length){

	uint16_t i;
	if(length % 4) return;
	
	__disable_irq();
	
	uint32_t page_size = NRF_FICR->CODEPAGESIZE;
	uint32_t *addr = (uint32_t *)(page_size * DATA_STORE_PAGE);
	
    // Turn on flash erase enable and wait until the NVMC is ready:
    NRF_NVMC->CONFIG = (NVMC_CONFIG_WEN_Een << NVMC_CONFIG_WEN_Pos);

    while (NRF_NVMC->READY == NVMC_READY_READY_Busy)
    {
        // Do nothing.
    }

    // Erase page:
    NRF_NVMC->ERASEPAGE = (uint32_t)addr;

    while (NRF_NVMC->READY == NVMC_READY_READY_Busy)
    {
        // Do nothing.
    }

    // Turn on flash write enable and wait until the NVMC is ready:
    NRF_NVMC->CONFIG = (NVMC_CONFIG_WEN_Wen << NVMC_CONFIG_WEN_Pos);

    while (NRF_NVMC->READY == NVMC_READY_READY_Busy)
    {
        // Do nothing.
    }

	
	for(i=0; i<length; i+=sizeof(uint32_t)){
		
		*addr = *p;
		
		while (NRF_NVMC->READY == NVMC_READY_READY_Busy)
		{
			// Do nothing.
		}
		
		addr++;
		p++;
	}
		
    while (NRF_NVMC->READY == NVMC_READY_READY_Busy)
    {
        // Do nothing.
    }

    // Turn off flash erase enable and wait until the NVMC is ready:
    NRF_NVMC->CONFIG = (NVMC_CONFIG_WEN_Ren << NVMC_CONFIG_WEN_Pos);

    while (NRF_NVMC->READY == NVMC_READY_READY_Busy)
    {
        // Do nothing.
    }
	
	__enable_irq();
}
