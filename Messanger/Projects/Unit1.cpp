//------------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop

#include <iostream>
#include <io.h>
#include <fcntl.h>
#include <sys\stat.h>
#include <cstring>
#include <string>
#include <string.h>
#include <fstream.h>

#include "Unit1.h"
//------------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TForm1 *Form1;
//------------------------------------------------------------------------------
//==============================================================================
//............................. Globalie mainigie .............................
//==============================================================================

#define BUFSIZE 255     //bufera lielums

char bufrd[BUFSIZE], bufwr[BUFSIZE]; //pienemsana un sutisanas buferi
using namespace std;

//------------------------------------------------------------------------------

HANDLE COMport;		//port

//структура OVERLAPPED vajag lai operaciji biji assinhronas (lasisanai un rakstisani vajag citadi)
OVERLAPPED overlapped;		//operacija lai lasit(ReadThread)
OVERLAPPED overlappedwr;       	//operacija lai rakstit (WriteThread)

//------------------------------------------------------------------------------

int handle;             	//vajag lai stradat ar failiem (ar io.h biblioteku)

//------------------------------------------------------------------------------

bool fl=0;	//flags kas paradijas vai rakstisanas oreaciaj ir veiksmiga

unsigned long counter;	//skaititajs pienemtas bitus


//==============================================================================
//............................ funkcijas ......................................
//==============================================================================

void COMOpen(void);             //atvert port
void COMClose(void);            //aizvert port

//==============================================================================
//.............................. Daudzplaisnes ................................
//==============================================================================

//---------------------------------------------------------------------------

//bitu pecteciba lasisana plusms no com porta uz bufer
class ReadThread : public TThread{
	private:
		void __fastcall Printing();	//izvade pienemtas baitus uz ekrans un faila
	protected:
		void __fastcall Execute();	//galvena plusmas funkcija
	public:
		__fastcall ReadThread(bool CreateSuspended);	//plusmas konstruktors
};

//------------------------------------------------------------------------------

//bitu pecteciba rakstisana plusms no bufera uz com porta
class WriteThread : public TThread{
	private:
		void __fastcall Printing();	//stavokla izvade ekrana
	protected:
		void __fastcall Execute();      //galvena plusmas funkcija
	public:
		__fastcall WriteThread(bool CreateSuspended);	//plusmas konstruktors
};

//------------------------------------------------------------------------------


//==============================================================================
//.............................. plusmas realizacija ...........................
//==============================================================================

//------------------------------------------------------------------------------
//............................... plusms ReadThead .............................
//------------------------------------------------------------------------------

ReadThread *reader;     //plusma ReadThread objekts

//------------------------------------------------------------------------------

//plusma ReadThread konstruktors, defolta ir tukss
__fastcall ReadThread::ReadThread(bool CreateSuspended) : TThread(CreateSuspended)
{}

//------------------------------------------------------------------------------

//galvena plusmas funkcija, realizeja baitu pieniemsanas no com porta
void __fastcall ReadThread::Execute(){
	COMSTAT comstat;		//porta tagadnes stavokla, seit ir lietots lai identificet com porta pienemtas baitu skaits
	DWORD btr, temp, mask, signal;	//temp ir lietots ka stub

	overlapped.hEvent = CreateEvent(NULL, true, true, NULL);	//izveidot signalo objekts - notikums par assinhronajam operacijam
	SetCommMask(COMport, EV_RXCHAR);                   	        //iestatit mask uz akcivaciju kad ir notikums baita pienemsanas com porta
	while(!Terminated){						//pagaidam plusms nav partraukts - veikt ciklu
		WaitCommEvent(COMport, &mask, &overlapped);               	// gaidit pienemsanas baita notikumu
		signal = WaitForSingleObject(overlapped.hEvent, INFINITE);	//apturet plusms lidz baita pienaca
		if(signal == WAIT_OBJECT_0){				        //ja pienacas baita notikums ir veiksmigs
			if(GetOverlappedResult(COMport, &overlapped, &temp, true)) //parbaudijam vai operacija WaitCommEvent ir veiksmigi pabeibzas
			if((mask & EV_RXCHAR)!=0){					//ja ir proti pienemsanas baita notikums
				ClearCommError(COMport, &temp, &comstat);		//vajag aizpildiet COMSTAT strukturu
				btr = comstat.cbInQue;     //un panemt no vinu cik baitu ir pienemts
				if(btr){                         			//ja tiesi ir baiti par lasisanu
					ReadFile(COMport, bufrd, btr, &temp, &overlapped);     //izlasit baitu no com porta uz programmas bufera
					counter+=btr;                                          //palielinat baitu skaititaju
					Synchronize(Printing);                      		//izraisit izvades faila un uz ekrans funkciju
				}
			}
		}
	}
	CloseHandle(overlapped.hEvent);		//pirms tam kad iziet no plusma aizver objekts - notikums
}

//------------------------------------------------------------------------------

//izvadam pienemtas baitus uz ekrana un faila (ja tas ir iesledzs)
void __fastcall ReadThread::Printing(){
	char y[7];
	memset(y, 0, 7);

	Form1->Memo1->Lines->Add("Opp:");
	Form1->Memo1->Lines->Add((char* )bufrd); //izvadam pienemtu rindu uz Memo
	strcat( y, "Opp: \n");

	if(Form1->CheckBox3->Checked == true)  {write(handle, y, strlen(y));  write(handle, bufrd, strlen(bufrd));}
	memset(bufrd, 0, BUFSIZE);	        //izteret bufer (lai dati nemainas)
}

//------------------------------------------------------------------------------

//==============================================================================
//............................... plusms WriteThead ............................
//==============================================================================

WriteThread *writer;     //plusma objekts WriteThread

//------------------------------------------------------------------------------

//plusma WriteThread konstruktors, defolta ir tukss
__fastcall WriteThread::WriteThread(bool CreateSuspended) : TThread(CreateSuspended)
{}

//------------------------------------------------------------------------------

//galvena plusma funkcija, veidoja baitu aizutisanu no bufera uz Com port
void __fastcall WriteThread::Execute(){
	DWORD temp, signal;	//temp ir lietots ka stub

	overlappedwr.hEvent = CreateEvent(NULL, true, true, NULL);   	  //izveidot notikums
	WriteFile(COMport, bufwr, strlen(bufwr), &temp, &overlappedwr);  //pasutit baitus uz port
	signal = WaitForSingleObject(overlappedwr.hEvent, INFINITE);	  //pause plusmu pagaidam operacija WriteFile pabeigts
	if((signal == WAIT_OBJECT_0) && (GetOverlappedResult(COMport, &overlappedwr, &temp, true))) fl = true; //ja operacija veiksmigi pabega darbu salit vajadzigu flag stavoklu
	else fl = false;
	Synchronize(Printing);	//izvadam operacijas staviklu stavokla rinda
	CloseHandle(overlappedwr.hEvent);	//pirms pam kad iziet aizert objekts - notikums
}

//------------------------------------------------------------------------------

//izvadam datus pasutisansa stavokla uz ekranu
void __fastcall WriteThread::Printing(){
	if(!fl){	//parbaudijam flag stavoklu
		Form1->StatusBar1->Panels->Items[0]->Text  = "Sending error";
		return;
	}
	Form1->StatusBar1->Panels->Items[0]->Text  = "Sending is successful";
}

//------------------------------------------------------------------------------


//==============================================================================
//............................. formas elementi ................................
//==============================================================================

//------------------------------------------------------------------------------

//formas konstruktors
__fastcall TForm1::TForm1(TComponent* Owner) : TForm(Owner){
	Form1->Label5->Enabled = false;
	Form1->Label6->Enabled = false;
	Form1->Button1->Enabled = false;
	Form1->CheckBox1->Enabled = false;
	Form1->CheckBox2->Enabled = false;
	Form1->Memo1->Text = "";
}

//------------------------------------------------------------------------------

//'Open port' poga
void __fastcall TForm1::SpeedButton1Click(TObject *Sender){
	if(SpeedButton1->Down){

		Form1->ComboBox1->Enabled = false;
		Form1->ComboBox2->Enabled = false;
		Form1->Button1->Enabled = true;
		Form1->CheckBox1->Enabled = true;
		Form1->CheckBox2->Enabled = true;

		Form1->SpeedButton1->Caption = "Close port";

		counter = 0;	//tiram baitu skaititaju

		//ja DTR un RTS biji izveleti
		Form1->CheckBox1Click(Sender);
		Form1->CheckBox2Click(Sender);
		COMOpen();
	}

	else{
		COMClose();

		Form1->SpeedButton1->Caption = "Open port";
		Form1->StatusBar1->Panels->Items[0]->Text = "";

		Form1->ComboBox1->Enabled = true;
		Form1->ComboBox2->Enabled = true;
		Form1->Button1->Enabled = false;
		Form1->CheckBox1->Enabled = false;
		Form1->CheckBox2->Enabled = false;
	}
}

//------------------------------------------------------------------------------

//Formas aizvertesana
void __fastcall TForm1::FormClose(TObject *Sender, TCloseAction &Action){
	if(reader)reader->Terminate(); 	//aizvert plusmu lasisanas no porta
	if(writer)writer->Terminate();         //aizvert plusma rakstisana uz port
	if(COMport)CloseHandle(COMport);       //aizvert plusmu
	if(handle)close(handle);               //aizvert failu uz kurieni dati ir rakstits
}
//------------------------------------------------------------------------------

//'Save to file' bultina
void __fastcall TForm1::CheckBox3Click(TObject *Sender){
	if(Form1->CheckBox3->Checked){
		Form1->Label5->Enabled = true;
		Form1->Label6->Enabled = true;
		Form1->Label7->Enabled = true;

		Form1->StatusBar1->Panels->Items[1]->Text = "Output into file!";
	}
	else{
		Form1->Label5->Enabled = false;
		Form1->Label6->Enabled = false;
		Form1->Label7->Enabled = false;

		Form1->StatusBar1->Panels->Items[1]->Text = "";
	}
}
//------------------------------------------------------------------------------

//'Send messnge' poga
void __fastcall TForm1::Button1Click(TObject *Sender){
	memset(bufwr,0,BUFSIZE);
	PurgeComm(COMport, PURGE_TXCLEAR);      //izteret pasutisanas bufers
	AnsiString ed_txt = Form1->Edit1->Text; //kopet datus no Edit uz mainigu ed_txt

	char y[7];
	memset(y, 0, 7);
	char* bufwr2 = ed_txt.c_str();          //kopet datus no ed_txt uz bufwr2
	strncpy(bufwr, bufwr2, strlen(bufwr2)); //no bufwr2 kopejam datus uz bufwr kads talat ir pasutits com porta

	Form1->Memo1->Lines->Add("You:");
	Form1->Memo1->Lines->Add((char* )bufwr);
	Form1->Memo1->Lines->Add("");

	strcat( y, "You: \n");
	strcat( bufwr, " \r\n");
	if(Form1->CheckBox3->Checked == true)  {write(handle, y, strlen(y));  write(handle, bufwr, strlen(bufwr));}

	writer = new WriteThread(false);               //izveidot un aktivet datus ierakstisanas uz port plusms
	writer->FreeOnTerminate = true;                //uztaisit tas lai plusms automatiski izcinats pec beigas

	Form1->Edit1->Text = "";
}

//------------------------------------------------------------------------------

//'Clear log' poga
void __fastcall TForm1::Button3Click(TObject *Sender){
	Form1->Memo1->Clear();
}

//------------------------------------------------------------------------------

//DTR bultina
void __fastcall TForm1::CheckBox1Click(TObject *Sender){
	if(Form1->CheckBox1->Checked) EscapeCommFunction(COMport, SETDTR);
	else EscapeCommFunction(COMport, CLRDTR);
}

//------------------------------------------------------------------------------

//RTS bultina
void __fastcall TForm1::CheckBox2Click(TObject *Sender){
	if(Form1->CheckBox2->Checked) EscapeCommFunction(COMport, SETRTS);
	else EscapeCommFunction(COMport, CLRRTS);
}

//------------------------------------------------------------------------------

//==============================================================================
//......................... funkcijas realizesana ..............................
//==============================================================================

//------------------------------------------------------------------------------

//com port atversanas un inicializesanas funkcija
void COMOpen(){

	String portname;     	//com port vards
	DCB dcb;                //DCB inicializesanas struktura
	COMMTIMEOUTS timeouts;

	portname = Form1->ComboBox1->Text;

	//atvert port par assinhronas operacijas vajag flag FILE_FLAG_OVERLAPPED
	COMport = CreateFile(portname.c_str(),GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);

	if(COMport == INVALID_HANDLE_VALUE){            //ja ir com port atversanas kluda
		Form1->SpeedButton1->Down = false;
		Form1->StatusBar1->Panels->Items[0]->Text = "Could not open the port";

		Form1->SpeedButton1->Caption = "Open port";

		Form1->ComboBox1->Enabled = true;
		Form1->ComboBox2->Enabled = true;
		Form1->Button1->Enabled = false;
		Form1->CheckBox1->Enabled = false;
		Form1->CheckBox2->Enabled = false;

		Application->Title = "!!!COM PORT OPEN FATAL ERROR!!!";
		ShowMessage("             This COM port does not exist or is unavailable! \n             Please choose another one.");

		COMClose();
		return;
	}

	//port inicializesana

	dcb.DCBlength = sizeof(DCB);

	 //nolasit DCB strukturu no port
	if(!GetCommState(COMport, &dcb)){	//ja nav izdarits
		COMClose();
		Form1->StatusBar1->Panels->Items[0]->Text  = "Failed to read DCB";
		return;
	}

	//DCB strukturas inicializacija
	dcb.BaudRate = StrToInt(Form1->ComboBox2->Text);       //rakstim parsutisanas atrums
	dcb.fBinary = TRUE;                                    //iesledzam binary rezims
	dcb.fOutxCtsFlow = FALSE;                              //aizsledzam skatities uz CTS
	dcb.fOutxDsrFlow = FALSE;                              //aizsledzam skatities uz DSR
	dcb.fDtrControl = DTR_CONTROL_DISABLE;                 //aizsledzam izmantosanas DTR liniju
	dcb.fDsrSensitivity = FALSE;
	dcb.fNull = FALSE;                                     //atlaut nuleto baitu pienemsanu
	dcb.fRtsControl = RTS_CONTROL_DISABLE;                 //aizsledzam izmantosanas RTS liniju
	dcb.fAbortOnError = FALSE;                             //aizsledzam visas rakstisanas vai lasisanas operacijas beigsanas ja ir kluda
	dcb.ByteSize = 8;                                      //8 bit viena baita
	dcb.Parity = 0;                                        //aizsledzan 'vai ir parais' parbaude
	dcb.StopBits = 0;                                      //rakstim viens stop-bits

	//augspiladet DCB strukturu uz port
	if(!SetCommState(COMport, &dcb)){	//ja neidevas to izdarit
		COMClose();
		Form1->StatusBar1->Panels->Items[0]->Text  = "Failed to set DCB";
		return;
	}

	//uztaisit timeouts
	timeouts.ReadIntervalTimeout = 0;	 	//taimouts starp 2 simbolam
	timeouts.ReadTotalTimeoutMultiplier = 0;	//kopigs lasisanas operacijas taimouts
	timeouts.ReadTotalTimeoutConstant = 0;         //konstanta par kopigo taimouta lasisanas operacija
	timeouts.WriteTotalTimeoutMultiplier = 0;      //kopigs rakstisanas operacijas taimouts
	timeouts.WriteTotalTimeoutConstant = 0;        //konstanta par kopigo taimouta rakstisana operacija

	 //ierakstit timeouts strukturu uz port
	if(!SetCommTimeouts(COMport, &timeouts)){	//ja neizdevas
		COMClose();
		Form1->StatusBar1->Panels->Items[0]->Text  = "Failed to set time-outs";
		return;
	}

	//uztaisit pienemsanas un sutisanas rindas izmers
	SetupComm(COMport,2000,2000);

	handle = open("MessangesHistory.txt", O_CREAT | O_APPEND |  O_TEXT | O_RDWR, S_IWRITE | S_IREAD );

	if(handle==-1){		//ja ir faila atversanas kluda
		Form1->StatusBar1->Panels->Items[1]->Text = "File openning error";
		Form1->Label6->Hide();
		Form1->CheckBox3->Checked = false;
		Form1->CheckBox3->Enabled = false;
	}
	else Form1->StatusBar1->Panels->Items[0]->Text = "Port successfully open";

	PurgeComm(COMport, PURGE_RXCLEAR);	//iztiret pienemsanas porta bufers

	reader = new ReadThread(false);	//uztaisit un saciet baitu lasisanas plusmu
	reader->FreeOnTerminate = true;    //uztaisit sito plusmu ipasibu

}

//------------------------------------------------------------------------------

//com porta aizvertesanas funkcija
void COMClose(){
	if(writer)writer->Terminate();		//aizvert plusma rakstisana uz port
	if(reader)reader->Terminate();         //aizvert plusmu lasisanas no port
	CloseHandle(COMport);                  //aizvert port
	COMport=0;
	close(handle);				//aizvert pinemtas datus rakstisanas fila
	handle=0;
}
//------------------------------------------------------------------------------

//'Mesanges recovery' poga
void __fastcall TForm1::Button2Click(TObject *Sender){
	AnsiString b;
	int ff = 0;
	string s;
	ifstream file("MessangesHistory.txt");

	while(getline(file, s)){ // izvadet visu failu pa 2 rindas uz memo
		ff++;
		b = s.c_str();
		Form1->Memo1->Lines->Add(b);

		if (ff == 2) {
			Form1->Memo1->Lines->Add("");
			ff = 0;
		}
	}

   file.close();
}
//------------------------------------------------------------------------------

//'Info' poga
void __fastcall TForm1::Button4Click(TObject *Sender){
	Application->Title = "Messanger iformation";
	ShowMessage("             This application was created by the student. \n             It is intended for messaging. \n             No rights are protected. Enjoy it! :)");
}
//------------------------------------------------------------------------------

//'?' poga
void __fastcall TForm1::Button5Click(TObject *Sender){
	Application->Title = "COM port open help";
	ShowMessage(" First, to get started, you need to select the COM port and baud    rate, \n then click on the open port button. \n Do not remove the DTR and RTS check marks \n for correct operation between all kinds of devices. \n If you want save the history of your chat to remain tick the 'save  to file' checkbox.");
}
