#include "gxid_proto.h"

int gxid_claim_device(char *host, int port, 
		      GxidU32 device, GxidU32 window, int exclusive);
int gxid_release_device(char *host, int port, GxidU32 device, 
			GxidU32 window);
