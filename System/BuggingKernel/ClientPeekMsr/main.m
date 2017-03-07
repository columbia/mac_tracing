//
//  main.m
//  PeekMsrClient
//
//  Created by weng on 2/23/17.
//  Copyright © 2017 weng. All rights reserved.
//

#import <Foundation/Foundation.h>
#include <stdlib.h>

#define kPeekMsrClassName "PeekMsr"
#define	MSR_IA32_LSTAR				0xC0000082

enum {
    kPeekMsrUserClientRDMSR = 0,
    kPeekMsrUserClientWRMSR = 1,
    kPeekMsrUserClientHKern = 2,
};

typedef struct {
    UInt32 msr;
    UInt32 param_hi;
    UInt32 param_lo;
} msrIOdata;


int main(int argc, const char * argv[])
{
	mach_port_t masterPort;
    io_iterator_t iter = 0;
    io_service_t service = 0;
    io_connect_t connection = 0;
    kern_return_t kr;
    
    msrIOdata in;
    msrIOdata out;
    size_t outsize;
    UInt32 selector;
    
    if (argc < 2)
        return 1;
    
    // Look up the object we wish to open. This example uses simple class
    // matching (IOServiceMatching()) to look up the object that is the
    // SamplePCI driver class instantiated by the kext.
	kr = IOMasterPort(MACH_PORT_NULL, &masterPort);
	if (kr != KERN_SUCCESS) {
		printf("Can't get masterport\n");
		goto out;
	}

    kr = IOServiceGetMatchingServices(masterPort, IOServiceMatching(kPeekMsrClassName), &iter);

    if (kr != KERN_SUCCESS) {
        printf("PeekMsr is not running %s\n", mach_error_string(kr));
        kr = -1;
        goto out;
    }
    
    service = IOIteratorNext(iter);
    io_string_t path;
    kr = IORegistryEntryGetPath(service, kIOServicePlane, path);
    if (kr != KERN_SUCCESS) {
        printf("PeekMsr is not running\n");
        goto out;
    }
    
    kr = IOServiceOpen(service, mach_task_self(), 0, &connection);
    if (kr != KERN_SUCCESS) {
        printf("Couldn't open IO Service\n");
        goto out;
    }
    
    // Test the user client
    if (!strncmp(argv[1], "read", 4)) {
        selector = kPeekMsrUserClientRDMSR;
        if (argc < 3) {
            kr = 1;
            goto out;
        }
        in.msr = (UInt32)strtoull(argv[2], NULL, 16);
        in.param_hi = in.param_lo = 0;
    } else if (!strncmp(argv[1], "khook", 5)) {
        selector = kPeekMsrUserClientHKern;
        in.msr = MSR_IA32_LSTAR;
        in.param_hi = in.param_lo = 0;
    } else if (!strncmp(argv[1], "write", 5)) {
        selector = kPeekMsrUserClientWRMSR;
        if (argc < 4) {
            kr = 1;
            goto out;
        }
        in.msr = (UInt32)strtoull(argv[2], NULL, 16);
        UInt64 val = (UInt64)strtoull(argv[3], NULL, 16);
        in.param_hi = (UInt32)(val >> 32);
        in.param_lo = (UInt32)val;
    }
    
    kr = IOConnectCallStructMethod(connection,
                                   selector,
                                   &in,
                                   sizeof(in),
                                   &out,
                                   &outsize);
    if (!strncmp(argv[1], "read", 4)) {
		printf("msr[%x] : 08%08x%08x\n", in.msr, out.param_hi, out.param_lo);
	}
    out:
    if (connection) {
        kr = IOServiceClose(connection);
        if (kr != KERN_SUCCESS)
            printf("IOServiceClose failed\n");
    }
    
    if (service)
        IOObjectRelease(service);
    
    if (iter)
        IOObjectRelease(iter);
    
    return kr;
}
