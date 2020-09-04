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
	t_atom		val;
	t_symbol	*name;
	void		*out;
	FT_HANDLE ftHandle;
	bool is_usb_connected;
	unsigned char DMXData[512];

} t_dummy;

///////////////////////// function prototypes
//// standard set
void *opendmxusb_new(t_symbol *s, long argc, t_atom *argv);
void opendmxusb_free(t_dummy *x);
void opendmxusb_assist(t_dummy *x, void *b, long m, long a, char *s);

void opendmxusb_int(t_dummy *x, long n);
void opendmxusb_float(t_dummy *x, double f);
void opendmxusb_anything(t_dummy *x, t_symbol *s, long ac, t_atom *av);
void opendmxusb_bang(t_dummy *x);
void opendmxusb_identify(t_dummy *x);
void opendmxusb_dblclick(t_dummy *x);
void opendmxusb_acant(t_dummy *x);

//////////////////////// global class pointer variable
void *opendmxusb_class;


void usb_connect(t_dummy* x);
void __fastcall execute(t_dummy* x);


void ext_main(void *r)
{
	t_class *c;

	c = class_new("opendmxusb", (method)opendmxusb_new, (method)opendmxusb_free, (long)sizeof(t_dummy),
				  0L /* leave NULL!! */, A_GIMME, 0);

	class_addmethod(c, (method)opendmxusb_bang,			"bang", 0);
	class_addmethod(c, (method)opendmxusb_int,			"int",		A_LONG, 0);
	class_addmethod(c, (method)opendmxusb_float,			"float",	A_FLOAT, 0);
	class_addmethod(c, (method)opendmxusb_anything,		"anything",	A_GIMME, 0);
	class_addmethod(c, (method)opendmxusb_identify,		"identify", 0);
	CLASS_METHOD_ATTR_PARSE(c, "identify", "undocumented", gensym("long"), 0, "1");

	// we want to 'reveal' the otherwise hidden 'xyzzy' method
	class_addmethod(c, (method)opendmxusb_anything,		"xyzzy", A_GIMME, 0);
	// here's an otherwise undocumented method, which does something that the user can't actually
	// do from the patcher however, we want them to know about it for some weird documentation reason.
	// so let's make it documentable. it won't appear in the quickref, because we can't send it from a message.
	class_addmethod(c, (method)opendmxusb_acant,			"blooop", A_CANT, 0);
	CLASS_METHOD_ATTR_PARSE(c, "blooop", "documentable", gensym("long"), 0, "1");

	/* you CAN'T call this from the patcher */
	class_addmethod(c, (method)opendmxusb_assist,			"assist",		A_CANT, 0);
	class_addmethod(c, (method)opendmxusb_dblclick,			"dblclick",		A_CANT, 0);

	CLASS_ATTR_SYM(c, "name", 0, t_dummy, name);

	class_register(CLASS_BOX, c);
	opendmxusb_class = c;
}

void opendmxusb_acant(t_dummy *x)
{
	object_post((t_object *)x, "can't touch this!");
}

void opendmxusb_assist(t_dummy *x, void *b, long m, long a, char *s)
{
	if (m == ASSIST_INLET) { //inlet
		sprintf(s, "I am inlet %ld", a);
	}
	else {	// outlet
		sprintf(s, "I am outlet %ld", a);
	}
}

void opendmxusb_free(t_dummy *x)
{
	FT_W32_CloseHandle(x->ftHandle);
	;
}

void opendmxusb_dblclick(t_dummy *x)
{
	object_post((t_object *)x, "I got a double-click");
}

void opendmxusb_int(t_dummy *x, long n)
{
	if (0 < n && n > 255)return;

	for (int i = 0; i < 512; i++) {
		x->DMXData[i] = n;
	}
	execute(x);
}

void opendmxusb_float(t_dummy *x, double f)
{
	atom_setfloat(&x->val, f);
	opendmxusb_bang(x);
}

void opendmxusb_anything(t_dummy *x, t_symbol *s, long ac, t_atom *av)
{
	if (s == gensym("xyzzy")) {
		object_post((t_object *)x, "A hollow voice says 'Plugh'");
	} else {
		atom_setsym(&x->val, s);
		opendmxusb_bang(x);
	}
}

void opendmxusb_bang(t_dummy *x)
{
	execute(x);
	/*switch (x->val.a_type) {
	case A_LONG: outlet_int(x->out, atom_getlong(&x->val)); break;
	case A_FLOAT: outlet_float(x->out, atom_getfloat(&x->val)); break;
	case A_SYM: outlet_anything(x->out, atom_getsym(&x->val), 0, NULL); break;
	default: break;
	}*/
}

void opendmxusb_identify(t_dummy *x)
{
	object_post((t_object *)x, "my name is %s", x->name->s_name);
}

void *opendmxusb_new(t_symbol *s, long argc, t_atom *argv)
{
	t_dummy *x = NULL;

	if ((x = (t_dummy *)object_alloc(opendmxusb_class))) {
		x->name = gensym("");
		if (argc && argv) {
			x->name = atom_getsym(argv);
		}
		if (!x->name || x->name == gensym(""))
			x->name = symbol_unique();

		atom_setlong(&x->val, 0);
		x->out = outlet_new(x, NULL);

		usb_connect(x);
	}
	return (x);
}


void usb_connect(t_dummy* x) {


	FT_STATUS ftStatus;
	char Buf[64];

	ftStatus = FT_ListDevices(0, Buf, FT_LIST_BY_INDEX | FT_OPEN_BY_DESCRIPTION);
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

	x->is_usb_connected = true;


}
void __fastcall execute(t_dummy* x)
{
	int i;
	ULONG bytesWritten;
	int StartCode = 0;

	// set RS485 for sendin
	FT_W32_EscapeCommFunction(x->ftHandle, CLRRTS);

	
	FT_W32_SetCommBreak(x->ftHandle);
	FT_W32_ClearCommBreak(x->ftHandle);

	FT_W32_WriteFile(x->ftHandle, &StartCode, 1, &bytesWritten, NULL);
	FT_W32_WriteFile(x->ftHandle, x->DMXData, 512, &bytesWritten, NULL);	

}