/* Ruby/SerialPort $Id$
 * Guillaume Pierronnet <moumar@netcourrier.com>
 * Alan Stern <stern@rowland.harvard.edu>
 * Daniel E. Shipton <dshipton@redshiptechnologies.com>
 *
 * This code is hereby licensed for public consumption under either the
 * GNU GPL v2 or greater.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * For documentation on serial programming, see the excellent:
 * "Serial Programming Guide for POSIX Operating Systems"
 * written Michael R. Sweet.
 * http://www.easysw.com/~mike/serial/
 */

#include <stdio.h>   /* Standard input/output definitions */
#include <io.h>      /* Low-level I/O definitions */
#include <fcntl.h>   /* File control definitions */
#include <windows.h> /* Windows standard function definitions */

#ifndef RB_SERIAL_EXPORT
#define RB_SERIAL_EXPORT __declspec(dllexport)
//#define RB_SERIAL_EXPORT 
#endif

static char sGetCommState[] = "GetCommState";
static char sSetCommState[] = "SetCommState";
static char sGetCommTimeouts[] = "GetCommTimeouts";
static char sSetCommTimeouts[] = "SetCommTimeouts";


static HANDLE get_handle_helper(obj)
   VALUE obj;
{
   OpenFile *fptr;

   GetOpenFile(obj, fptr);
   return (HANDLE) _get_osfhandle(fileno(fptr->f));
}

VALUE RB_SERIAL_EXPORT sp_create_impl(class, _port)
   VALUE class, _port;
{
   OpenFile *fp;
   int fd;
   HANDLE fh;
   int num_port;
   char *port;
   char *ports[] = {
      "COM1", "COM2", "COM3", "COM4",
      "COM5", "COM6", "COM7", "COM8"
   };
   //int new_fd;

   DCB dcb;

   NEWOBJ(sp, struct RFile);
   rb_secure(4);
   OBJSETUP(sp, class, T_FILE);
   MakeOpenFile((VALUE) sp, fp);

   switch(TYPE(_port))
   {
      case T_FIXNUM:
         num_port = FIX2INT(_port);
         if (num_port < 0 || num_port > sizeof(ports) / sizeof(ports[0]))
         {
            rb_raise(rb_eArgError, "illegal port number");
         }
         port = ports[num_port];
         break;

      case T_STRING:
         Check_SafeStr(_port);
         port = RSTRING(_port)->ptr;
         break;

      default:
         rb_raise(rb_eTypeError, "wrong argument type");
         break;
   }

   printf("SerialPort => %s\n", port);
   fd = open(port, O_BINARY | O_RDWR);
   printf("        fd => %i\n", fd);
   if (fd == -1)
      rb_sys_fail(port);
   fh = (HANDLE) _get_osfhandle(fd);
   printf("        fh => %i\n", fh);
   if (SetupComm(fh, 1024, 1024) == 0)
   {
      close(fd);
      rb_raise(rb_eArgError, "not a serial port");
   }

   dcb.DCBlength = sizeof(dcb);
   if (GetCommState(fh, &dcb) == 0)
   {
      close(fd);
      rb_sys_fail(sGetCommState);
   }
   dcb.fBinary = TRUE;
   dcb.fParity = FALSE;
   dcb.fOutxDsrFlow = FALSE;
   dcb.fDtrControl = DTR_CONTROL_ENABLE;
   dcb.fDsrSensitivity = FALSE;
   dcb.fTXContinueOnXoff = FALSE;
   dcb.fErrorChar = FALSE;
   dcb.fNull = FALSE;
   dcb.fAbortOnError = FALSE;
   dcb.XonChar = 17;
   dcb.XoffChar = 19;
   if (SetCommState(fh, &dcb) == 0)
   {
      close(fd);
      rb_sys_fail(sSetCommState);
   }

   errno = 0;
   fp->mode = FMODE_READWRITE | FMODE_BINMODE | FMODE_SYNC;
   fp->f = fdopen(fd, "rb+");
   return (VALUE) sp;
}

VALUE RB_SERIAL_EXPORT sp_set_modem_params_impl(argc, argv, self)
   int argc;
   VALUE *argv, self;
{
   HANDLE fh;
   DCB dcb;
   VALUE _data_rate, _data_bits, _parity, _stop_bits;
   int use_hash = 0;
   int data_rate, data_bits, parity;

   if (argc == 0)
   {
      return self;
   }
   if (argc == 1 && T_HASH == TYPE(argv[0]))
   {
      use_hash = 1;
      _data_rate = rb_hash_aref(argv[0], sBaud);
      _data_bits = rb_hash_aref(argv[0], sDataBits);
      _stop_bits = rb_hash_aref(argv[0], sStopBits);
      _parity = rb_hash_aref(argv[0], sParity);
   }

   fh = get_handle_helper(self);
   dcb.DCBlength = sizeof(dcb);
   if (GetCommState(fh, &dcb) == 0)
   {
      rb_sys_fail(sGetCommState);
   }

   if (!use_hash)
   {
      _data_rate = argv[0];
   }

   if (NIL_P(_data_rate))
   {
      goto SkipDataRate;
   }

   Check_Type(_data_rate, T_FIXNUM);

   data_rate = FIX2INT(_data_rate);
   switch (data_rate)
   {
      case 110:
      case 300:
      case 600:
      case 1200:
      case 2400:
      case 4800:
      case 9600:
      case 14400:
      case 19200:
      case 38400:
      case 56000:
      case 57600:
      case 115200:
      case 128000:
      case 256000:
         dcb.BaudRate = data_rate;
         break;

      default:
         rb_raise(rb_eArgError, "unknown baud rate");
         break;
   }

   SkipDataRate:

   if (!use_hash)
   {
      _data_bits = (argc >= 2 ? argv[1] : INT2FIX(8));
   }

   if (NIL_P(_data_bits))
   {
      goto SkipDataBits;
   }

   Check_Type(_data_bits, T_FIXNUM);

   data_bits = FIX2INT(_data_bits);
   if (4 <= data_bits && data_bits <= 8)
   {
      dcb.ByteSize = data_bits;
   }
   else
   {
      rb_raise(rb_eArgError, "unknown character size");
   }

   SkipDataBits:

   if (!use_hash)
   {
      _stop_bits = (argc >= 3 ? argv[2] : INT2FIX(1));
   }

   if (NIL_P(_stop_bits))
   {
      goto SkipStopBits;
   }

   Check_Type(_stop_bits, T_FIXNUM);

   switch (FIX2INT(_stop_bits))
   {
      case 1:
         dcb.StopBits = ONESTOPBIT;
         break;
      case 2:
         dcb.StopBits = TWOSTOPBITS;
         break;
      default:
         rb_raise(rb_eArgError, "unknown number of stop bits");
         break;
   }

   SkipStopBits:

   if (!use_hash)
   {
      _parity = (argc >= 4 ? argv[3] : (dcb.ByteSize == 8 ?
               INT2FIX(NOPARITY) : INT2FIX(EVENPARITY)));
   }

   if (NIL_P(_parity))
   {
      goto SkipParity;
   }

   Check_Type(_parity, T_FIXNUM);

   parity = FIX2INT(_parity);
   switch (parity)
   {
      case EVENPARITY:
      case ODDPARITY:
      case MARKPARITY:
      case SPACEPARITY:
      case NOPARITY:
         dcb.Parity = parity;
         break;

      default:
         rb_raise(rb_eArgError, "unknown parity");
         break;
   }

   SkipParity:

   if (SetCommState(fh, &dcb) == 0)
   {
      rb_sys_fail(sSetCommState);
   }

   return argv[0];
}

void RB_SERIAL_EXPORT get_modem_params_impl(self, mp)
   VALUE self;
   struct modem_params *mp;
{
   HANDLE fh;
   DCB dcb;

   fh = get_handle_helper(self);
   dcb.DCBlength = sizeof(dcb);
   if (GetCommState(fh, &dcb) == 0)
   {
      rb_sys_fail(sGetCommState);
   }

   mp->data_rate = dcb.BaudRate;
   mp->data_bits = dcb.ByteSize;
   mp->stop_bits = (dcb.StopBits == ONESTOPBIT ? 1 : 2);
   mp->parity = dcb.Parity;
}

VALUE RB_SERIAL_EXPORT sp_set_flow_control_impl(self, val)
   VALUE self, val;
{
   HANDLE fh;
   int flowc;
   DCB dcb;

   Check_Type(val, T_FIXNUM);

   fh = get_handle_helper(self);
   dcb.DCBlength = sizeof(dcb);
   if (GetCommState(fh, &dcb) == 0)
   {
      rb_sys_fail(sGetCommState);
   }

   flowc = FIX2INT(val);
   if (flowc & HARD)
   {
      dcb.fRtsControl = RTS_CONTROL_HANDSHAKE;
      dcb.fOutxCtsFlow = TRUE;
   }
   else
   {
      dcb.fRtsControl = RTS_CONTROL_ENABLE;
      dcb.fOutxCtsFlow = FALSE;
   }

   if (flowc & SOFT)
   {
      dcb.fOutX = dcb.fInX = TRUE;
   }
   else
   {
      dcb.fOutX = dcb.fInX = FALSE;
   }

   if (SetCommState(fh, &dcb) == 0)
   {
      rb_sys_fail(sSetCommState);
   }

   return val;
}

VALUE RB_SERIAL_EXPORT sp_get_flow_control_impl(self)
   VALUE self;
{
   HANDLE fh;
   int ret;
   DCB dcb;

   fh = get_handle_helper(self);
   dcb.DCBlength = sizeof(dcb);
   if (GetCommState(fh, &dcb) == 0)
   {
      rb_sys_fail(sGetCommState);
   }

   ret = 0;
   if (dcb.fOutxCtsFlow)
   {
      ret += HARD;
   }

   if (dcb.fOutX)
   {
      ret += SOFT;
   }

   return INT2FIX(ret);
}

VALUE RB_SERIAL_EXPORT sp_set_read_timeout_impl(self, val)
   VALUE self, val;
{
   int timeout;
   HANDLE fh;
   COMMTIMEOUTS ctout;

   Check_Type(val, T_FIXNUM);
   timeout = FIX2INT(val);

   fh = get_handle_helper(self);
   if (GetCommTimeouts(fh, &ctout) == 0)
   {
      rb_sys_fail(sGetCommTimeouts);
   }

   if (timeout < 0)
   {
      ctout.ReadIntervalTimeout = MAXDWORD;
      ctout.ReadTotalTimeoutMultiplier = 0;
      ctout.ReadTotalTimeoutConstant = 0;
   }
   else if (timeout == 0)
   {
      ctout.ReadIntervalTimeout = MAXDWORD;
      ctout.ReadTotalTimeoutMultiplier = MAXDWORD;
      ctout.ReadTotalTimeoutConstant = MAXDWORD - 1;
   }
   else
   {
      ctout.ReadIntervalTimeout = timeout;
      ctout.ReadTotalTimeoutMultiplier = 0;
      ctout.ReadTotalTimeoutConstant = timeout;
   }

   if (SetCommTimeouts(fh, &ctout) == 0)
   {
      rb_sys_fail(sSetCommTimeouts);
   }

   return val;
}

VALUE RB_SERIAL_EXPORT sp_get_read_timeout_impl(self)
   VALUE self;
{
   HANDLE fh;
   COMMTIMEOUTS ctout;

   fh = get_handle_helper(self);
   if (GetCommTimeouts(fh, &ctout) == 0)
   {
      rb_sys_fail(sGetCommTimeouts);
   }

   switch (ctout.ReadTotalTimeoutConstant)
   {
      case 0:
         return INT2FIX(-1);
      case MAXDWORD:
         return INT2FIX(0);
   }

   return INT2FIX(ctout.ReadTotalTimeoutConstant);
}

VALUE RB_SERIAL_EXPORT sp_set_write_timeout_impl(self, val)
   VALUE self, val;
{
   int timeout;
   HANDLE fh;
   COMMTIMEOUTS ctout;

   Check_Type(val, T_FIXNUM);
   timeout = FIX2INT(val);

   fh = get_handle_helper(self);
   if (GetCommTimeouts(fh, &ctout) == 0)
   {
      rb_sys_fail(sGetCommTimeouts);
   }

   if (timeout <= 0)
   {
      ctout.WriteTotalTimeoutMultiplier = 0;
      ctout.WriteTotalTimeoutConstant = 0;
   }
   else
   {
      ctout.WriteTotalTimeoutMultiplier = timeout;
      ctout.WriteTotalTimeoutConstant = 0;
   }

   if (SetCommTimeouts(fh, &ctout) == 0)
   {
      rb_sys_fail(sSetCommTimeouts);
   }

   return val;
}

VALUE RB_SERIAL_EXPORT sp_get_write_timeout_impl(self)
   VALUE self;
{
   HANDLE fh;
   COMMTIMEOUTS ctout;

   fh = get_handle_helper(self);
   if (GetCommTimeouts(fh, &ctout) == 0)
   {
      rb_sys_fail(sGetCommTimeouts);
   }

   return INT2FIX(ctout.WriteTotalTimeoutMultiplier);
}

static void delay_ms(time)
   int time;
{
   HANDLE ev;

   ev = CreateEvent(NULL, FALSE, FALSE, NULL);
   if (!ev)
   {
      rb_sys_fail("CreateEvent");
   }

   if (WaitForSingleObject(ev, time) == WAIT_FAILED)
   {
      rb_sys_fail("WaitForSingleObject");
   }

   CloseHandle(ev);
}

VALUE RB_SERIAL_EXPORT sp_break_impl(self, time)
   VALUE self, time;
{
   HANDLE fh;

   Check_Type(time, T_FIXNUM);

   fh = get_handle_helper(self);
   if (SetCommBreak(fh) == 0)
   {
      rb_sys_fail("SetCommBreak");
   }

   delay_ms(FIX2INT(time) * 100);
   ClearCommBreak(fh);

   return Qnil;
}

void RB_SERIAL_EXPORT get_line_signals_helper(obj, ls)
   VALUE obj;
   struct line_signals *ls;
{
   HANDLE fh;
   int status;

   fh = get_handle_helper(obj);
   if (GetCommModemStatus(fh, &status) == 0)
   {
      rb_sys_fail("GetCommModemStatus");
   }

   ls->cts = (status & MS_CTS_ON ? 1 : 0);
   ls->dsr = (status & MS_DSR_ON ? 1 : 0);
   ls->dcd = (status & MS_RLSD_ON ? 1 : 0);
   ls->ri  = (status & MS_RING_ON ? 1 : 0);
}

static VALUE set_signal(obj, val, sigoff, sigon)
   VALUE obj,val;
   int sigoff, sigon;
{
   HANDLE fh;
   int set, sig;

   Check_Type(val, T_FIXNUM);
   fh = get_handle_helper(obj);

   set = FIX2INT(val);
   if (set == 0)
   {
      sig = sigoff;
   }
   else if (set == 1)
   {
      sig = sigon;
   }
   else
   {
      rb_raise(rb_eArgError, "invalid value");
   }

   if (EscapeCommFunction(fh, sig) == 0)
   {
      rb_sys_fail("EscapeCommFunction");
   }

   return val;
}

VALUE RB_SERIAL_EXPORT sp_set_rts_impl(self, val)
   VALUE self, val;
{
   return set_signal(self, val, CLRRTS, SETRTS);
}

VALUE RB_SERIAL_EXPORT sp_set_dtr_impl(self, val)
   VALUE self, val;
{
   return set_signal(self, val, CLRDTR, SETDTR);
}

VALUE RB_SERIAL_EXPORT sp_get_rts_impl(self)
   VALUE self;
{
   rb_notimplement();
   return self;
}

VALUE RB_SERIAL_EXPORT sp_get_dtr_impl(self)
   VALUE self;
{
   rb_notimplement();
   return self;
}

