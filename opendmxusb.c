/**
	@file
	dummy - a dummy object
	jeremy bernstein - jeremy@bootsquad.com

	@ingroup	examples
*/

#include "ext.h"							// standard Max include, always required
#include "ext_obex.h"						// required for new style Max object

#include "FTD2XX.h"

////////////////////////// object struct
typedef struct opendmxusb
{
	t_object	ob;
	void		*out;
	FT_HANDLE ftHandle;
	bool is_usb_connected;
	unsigned char DMXData[512];
	t_atom outlist[512];
	unsigned int numCh;

} t_dummy;

///////////////////////// function prototypes
//// standard set
void *opendmxusb_new(t_symbol *s, long argc, t_atom *argv);
void opendmxusb_free(t_dummy *x);
void opendmxusb_assist(t_dummy *x, void *b, long m, long a, char *s);

void opendmxusb_int(t_dummy *x, long n);
void opendmxusb_list(t_dummy* x, t_symbol* s, long argc, t_atom* argv);
void opendmxusb_anything(t_dummy *x, t_symbol *s, long ac, t_atom *av);
void opendmxusb_bang(t_dummy *x);

void usb_connect(t_dummy* x);
void usb_close(t_dummy* x);
void __fastcall opendmxusb_send(t_dummy* x);


//////////////////////// global class pointer variable
void *opendmxusb_class;




void ext_main(void *r)
{
	t_class *c;

	c = class_new("opendmxusb", (method)opendmxusb_new, (method)opendmxusb_free, (long)sizeof(t_dummy),
				  0L /* leave NULL!! */, A_GIMME, 0);

	class_addmethod(c, (method)opendmxusb_bang,			"bang", 0);
	class_addmethod(c, (method)opendmxusb_int,			"int",		A_LONG, 0);
	class_addmethod(c, (method)opendmxusb_list, "list", A_GIMME, 0);
	class_addmethod(c, (method)opendmxusb_anything,		"anything",	A_GIMME, 0);
	class_addmethod(c, (method)usb_connect, "open",A_DEFSYM, 0);
	class_addmethod(c, (method)usb_close, "close", A_DEFSYM, 0);

	/* Inspector items ARE ATTRIBUTES */
	CLASS_ATTR_LONG(c, "numCh", 0, t_dummy, numCh);
	CLASS_ATTR_SAVE(c, "numCh", 0);
	// clip max value to 1-512
	CLASS_ATTR_FILTER_CLIP(c, "numCh", 1, 512);

	class_register(CLASS_BOX, c);
	opendmxusb_class = c;
}

void opendmxusb_assist(t_dummy *x, void *b, long m, long a, char *s)
{
	if (m == ASSIST_INLET) { //inlet
		sprintf(s, "I am inlet %ld", a);
	}
	else {	// outlet
		sprintf(s, "dmx values");
	}
}

void opendmxusb_free(t_dummy *x)
{
	usb_close(x);
}

void opendmxusb_int(t_dummy *x, long n)
{
	if (0 < n && n > 255)return;

	for (int i = 0; i < 512; i++) {
		x->DMXData[i] = n;
	}
}


void opendmxusb_list(t_dummy* x, t_symbol* s, long argc, t_atom* argv) {

	opendmxusb_anything(x, NULL, argc, argv);
}

void opendmxusb_anything(t_dummy *x, t_symbol *s, long argc, t_atom *argv)
{
	t_atom* ap;
	int i;
	// increment ap each time to get to the next atom
	for (i = 0, ap = argv; i < argc; i++, ap++) {
		switch (atom_gettype(ap)) {
		case A_LONG:
			x->DMXData[i] = atom_getlong(ap);
			break;
		case A_FLOAT:
			x->DMXData[i] = (byte)atom_getfloat(ap);
			break;
		}
	}
}

void opendmxusb_bang(t_dummy* x)
{
	opendmxusb_send(x);

	short i;
	for (i = 0; i < x->numCh; i++) {
		atom_setlong(x->outlist + i, x->DMXData[i]);
	}
	outlet_anything(x->out, gensym("list"),(short)x->numCh,x->outlist);
}


void *opendmxusb_new(t_symbol *s, long argc, t_atom *argv)
{
	t_dummy *x = NULL;

	if ((x = (t_dummy *)object_alloc(opendmxusb_class))) {

		object_post((t_object*)x, "initialize");

		x->out = listout(x);
		x->numCh = 512;
		int i = 0;
		for (i = 0; i < argc; i++) {
			if ((argv + i)->a_type == A_LONG) {
				x->numCh = atom_getlong(argv + i);
				
			}
		}
		object_post((t_object*)x, "DMX ch %ld", x->numCh);
		usb_connect(x);
	}
	return (x);
}


void usb_connect(t_dummy* x) {


	if (x->is_usb_connected) {
		object_post((t_object*)x, "Already connected");
		return;
	}
	FT_STATUS ftStatus;
	char Buf[64];

	ftStatus = FT_ListDevices(0, Buf, FT_LIST_BY_INDEX | FT_OPEN_BY_DESCRIPTION);
	if (ftStatus != FT_OK) {
		object_post((t_object*)x, "Device not found");
		return;
	}

	object_post((t_object*)x, "Found a device '%s'", Buf);

	x->ftHandle = FT_W32_CreateFile(Buf, GENERIC_READ | GENERIC_WRITE, 0, 0, \
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FT_OPEN_BY_DESCRIPTION, 0);

	// connect to first device
	if (x->ftHandle == INVALID_HANDLE_VALUE) {
		object_post((t_object*)x, "Error No deivice");
		//Application->MessageBoxA("No device", "Error", MB_OK);
		return;
	}

	FTDCB ftDCB;
	if (FT_W32_GetCommState(x->ftHandle, &ftDCB)) {
		// FT_W32_GetCommState ok, device state is in ftDCB
		ftDCB.BaudRate = 250000;
		ftDCB.Parity = FT_PARITY_NONE;
		ftDCB.StopBits = FT_STOP_BITS_2;
		ftDCB.ByteSize = FT_BITS_8;
		ftDCB.fOutX = false;
		ftDCB.fInX = false;
		ftDCB.fErrorChar = false;
		ftDCB.fBinary = true;
		ftDCB.fRtsControl = false;
		ftDCB.fAbortOnError = false;

		if (!FT_W32_SetCommState(x->ftHandle, &ftDCB)) {
			object_post((t_object*)x, "Set baud rate error");
			//				Application->MessageBoxA("Set baud rate error", "Error", MB_OK);
			return;
		}
	}

	FT_W32_PurgeComm(x->ftHandle, FT_PURGE_TX | FT_PURGE_RX);

	Sleep(1000L);
	object_post((t_object*)x, "Open %s",Buf);

	x->is_usb_connected = true;


}
void usb_close(t_dummy* x) {

	FT_W32_CloseHandle(x->ftHandle);
	object_post((t_object*)x, "Close");
	x->is_usb_connected = false;
}


void __fastcall opendmxusb_send(t_dummy* x)
{
	if (!x->is_usb_connected)return;
	int i;
	ULONG bytesWritten;
	int StartCode = 0;

	// set RS485 for sendin
	FT_W32_EscapeCommFunction(x->ftHandle, CLRRTS);

	
	FT_W32_SetCommBreak(x->ftHandle);
	FT_W32_ClearCommBreak(x->ftHandle);

	FT_W32_WriteFile(x->ftHandle, &StartCode, 1, &bytesWritten, NULL);
	FT_W32_WriteFile(x->ftHandle, x->DMXData, x->numCh, &bytesWritten, NULL);	

}