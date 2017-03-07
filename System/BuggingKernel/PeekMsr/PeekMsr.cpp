#include "PeekMsr.hpp"

// This required macro defines the class's constructors, destructors,
// and several other methods I/O Kit requires.
OSDefineMetaClassAndStructors(PeekMsr, IOService)

// Define the driver's superclass.
#define super IOService

bool PeekMsr::init(OSDictionary *dict)
{
	bool result = super::init(dict);
	IOLog("PeekMsr: Initializing\n");
	return result;
}

void PeekMsr::free(void)
{
	IOLog("PeekMsr: Freeing\n");
	super::free();
}

IOService *PeekMsr::probe(IOService *provider, SInt32 *score)
{
	IOService *result = super::probe(provider, score);
	IOLog("PeekMsr: Probing\n");
	return result;
}

bool PeekMsr::start(IOService *provider)
{
	bool result = super::start(provider);
	registerService();
	IOLog("PeekMsr: Starting\n");
	return result;
}

void PeekMsr::stop(IOService *provider)
{
	IOLog("PeekMsr: Stopping\n");
	super::stop(provider);
}

IOReturn PeekMsr::newUserClient(task_t owningTask, void * securityID, UInt32 type, IOUserClient ** handler)
{
	IOReturn ret =  kIOReturnSuccess;
	PeekMsrUserClient *client  = NULL;

	if (mClientCount >= MAXUSERS) {
		IOLog("PeekMsr: All Client slots are taken\n");
		return kIOReturnNoResources;
	}

	UInt8 index  = 0;
	for (index = 0; index < MAXUSERS; index++) {
		if (mClientArray[index] == NULL) {
			mClientArray[index] = (PeekMsrUserClient*)0xbeefbeef;
			break;
		}
	} //should be atomic??

	if (index == MAXUSERS) {
		IOLog("PeekMsr: All Client slots are taken\n");
		return kIOReturnNoResources;
	}
	
	client = OSDynamicCast(PeekMsrUserClient, OSMetaClass::allocClassWithName("PeekMsrUserClient"));
	if (!client) {
		mClientArray[index] = NULL;
		IOLog("PeekMsr::newUserClient: Can't create user client\n");
		return kIOReturnNoMemory;
	}
	
	if (!client->init()) {
		mClientArray[index] = NULL;
		client->release();
		client = NULL;
		IOLog("PeekMsr::newUserClient: Can't init user client\n");
		return kIOReturnError;
	} 

	// Start the client so it can accept requests.
	client->fTask = owningTask;
	client->attach(this);
	if (client->start(this) && (UInt64)mClientArray[index] == 0xbeefbeef) {
		mClientArray[index] = client;
		*handler = client;
		OSIncrementAtomic(&mClientCount);
		//mClientCount++; //TODO: should be atomic ??
		IOLog("PeekMsr::newUserClient: %d client = 0x%08x%08x\n", 
				index, (uint32_t)((uint64_t)client >> 32), (uint32_t)((uint64_t)client));
	} else {
	   	client->detach(this);					   
	   	client->release();						  
		mClientArray[index] = NULL;
		ret = kIOReturnError;
		IOLog("PeekMsr::newUserClient: Can't start user client\n");
	}

	return ret;
}

IOReturn PeekMsr::closeUserClient(PeekMsrUserClient *client)
{
	IOReturn ioReturn = kIOReturnBadArgument;
	UInt8 i = 0;

	IOLog("PeekMsr::CloseUserClient: 0x%08x%08x\n", (uint32_t)((uint64_t)client >> 32), (uint32_t)((uint64_t)client));
	for (i = 0; i < MAXUSERS; i++) {
		IOLog("PeekMsr::CloseUserClient checking userclient %d 0x%08x%08x\n", 
			i, (uint32_t)((uint64_t)mClientArray[i] >> 32), (uint32_t)((uint64_t)mClientArray[i]));
		if (mClientArray[i] == client) {
			mClientArray[i] = NULL;
			OSDecrementAtomic(&mClientCount);
			//mClientCount--; //TODO: should be atomic??
			ioReturn = kIOReturnSuccess;
			break;
		}
	}
	if (ioReturn != kIOReturnSuccess)
		IOLog("PeekMsr::CloseUserClient: No clients available to close\n");
	return ioReturn; 
}

#define rdmsr(msr,lo,hi) \
__asm__ volatile("rdmsr" : "=a" (lo), "=d" (hi) : "c" (msr))

#define wrmsr(msr,lo,hi) \
__asm__ volatile("wrmsr" : : "c" (msr), "a" (lo), "d" (hi))

// CPUID??
void PeekMsr::a_rdmsr(uint32_t msr, uint32_t *lo, uint32_t *hi)
{
	uint32_t _lo = 0, _hi = 0;
	rdmsr(msr, _lo, _hi);
	#ifdef DEBUG
	IOLog("rdmsr64 %x, %08x %08x\n", msr, _hi, _lo);
	#endif
	*lo = _lo;
	*hi = _hi;
}

void PeekMsr::a_wrmsr(uint32_t msr, uint32_t lo, uint32_t hi)
{
	wrmsr(msr, lo, hi);
	#ifdef DEBUG
	IOLog("wrmsr64 %x, %08x %08x\n", msr, hi, lo);
	#endif
}
