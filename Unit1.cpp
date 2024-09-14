//---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop

#include "Unit1.h"
#include  "FileCtrl.hpp"
#include <math.h>
#include <stdio.h>
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
#define USE_HT3310 1
TForm1 *Form1;
//123
    /*注意，雖然實際須使用符點運算，但因為誤差值的關係，所以統統*1000，採用整數運算.*/
    int   nCycleTime;
    int   tCAS_ns;
    int   tRP_ns;
    int   tRCD_ns;
    int   tRAS_ns;
    int   tFAW_ns;
    int   tRC_ns;
    int   tRFC_ns;
    int   tRRD_S_ns;
    int   tRRD_L_ns;
	int   ChCol,ChRow;
	
	char t704;//vpp
    char t705,t706;//vdd,vDDQ
	char t709,t710,t717,t718,t719,t720,t721,t722,t723,t724,t725,t726,t727,t728;//tckavg,taa,trcd,trp,tras,trc,twr
    char t729,t730,t731,t732,t733,t734; //trfc1,trfc2,trfcsb
    char t711,t712,t713,t714,t715;//tcas latency support

    unsigned char buffer[3264];  //1632*2
    unsigned char Txt_Buf[2176]; //1088*2
    unsigned char SPD_Buf[2048]; 
    unsigned char WrSPDBuf[3264];//1632*2
	unsigned char *buffer_temp1;
    FILE *fps;
    bool _bGetColor;
	bool _bNADimm;
    AnsiString BinFileName;
	
char TForm1::nibbleX(char x){
    if(x>=0x30 && x<=0x39)return x-0x30;
    if(x>=0x41 && x<=0x46)return x-0x37;
    if(x>=0x61 && x<=0x66)return x-0x57;
    return 0;
}

char TForm1::mstrToByte(PUCHAR DataBuffer,int index){
    char ByteX=0;
    ByteX=nibbleX(DataBuffer[index])<<4;
    ByteX = ByteX | nibbleX(DataBuffer[index+1]);
    return ByteX;
}
//---------------------------------------------------------------------------
__fastcall TForm1::TForm1(TComponent* Owner)
    : TForm(Owner)
{
    if(USE_HT3310)RadioSPD->Checked=true;
	StringGrid1->Refresh();  //stringGrid清空
    buffer_temp1 = new char [2064];
    memset(buffer_temp1,0x0,2064);
}
//---------------------------------------------------------------------------

void __fastcall TForm1::FormCreate(TObject *Sender)
{
    _bGetColor=false;
    this->Top=(Screen->Height-this->Height)/2;
    this->Left=(Screen->Width-this->Width)/2;

    DWord fstyle1,fstyle2,fstyle3;
    fstyle1 = GetWindowLong(Edit_tRFC->Handle, GWL_STYLE); //宣告Edit1輸入僅為數字
    SetWindowLong(Edit_tRFC->Handle, GWL_STYLE, fstyle1 | ES_NUMBER | ES_LEFT); // fstyle or ES_NUMBER

	fstyle2 = GetWindowLong(Edit_tRFC2->Handle, GWL_STYLE); //宣告Edit2輸入僅為數字
    SetWindowLong(Edit_tRFC2->Handle, GWL_STYLE, fstyle2 | ES_NUMBER | ES_LEFT); // fstyle or ES_NUMBER

    fstyle3 = GetWindowLong(Edit_tRFCsb->Handle, GWL_STYLE); //宣告Editsb輸入僅為數字
    SetWindowLong(Edit_tRFCsb->Handle, GWL_STYLE, fstyle3 | ES_NUMBER | ES_LEFT); // fstyle or ES_NUMBER
    
    Image_Close_Over->Left=-2000;
    Image_Close_Down->Left=-2000;
    Image_Close_Normal->Left=-2000;
    Img_mini1->Left=-2000;
    Img_mini2->Left=-2000;
    Img_mini3->Left=-2000;
    Img_mini4->Left=-2000;
    Img_mini5->Left=-2000;
}

//---------------------------------------------------------------------------
void __fastcall TForm1::Btn_Create2Click(TObject *Sender)
{
	ClearGrid();
	int Val=StrToInt(Combo_tCLKAVG->Text);
	nCycleTime=(int)(1000000/(Val/2));  //多*1000 ==>小數點後三位變成整數
	//ShowMessage( "頻率=" +IntToStr(Val));
	//-------------------   t704(Vpp)  -----------------------/
    if(Combo_VPP->ItemIndex==-1){
        StatusB->Panels->Items[0]->Text=("電壓vpp值設定錯誤!");
        return;
    }
    Val=110+(Combo_VPP->ItemIndex); //Combo_Vpp->ItemIndex = 25(if 設定電壓=1.35v)
    //ShowMessage( "val="+IntToStr(Val)); //Val = 135
    t704=(((Val-100)*2)/10)|0x20; //(135-100)*2 | 0x07 = 0x27
	//ShowMessage( "t704="+IntToStr(t704)); //27
	Edit_VPP->Text=IntToHex(t704,2);

	//-------------------   t705,t706(VDD,VDDQ)  -----------------------/
    if(Combo_Vol->ItemIndex==-1){
        StatusB->Panels->Items[0]->Text=("電壓值設定錯誤!");
        return;
    }
    Val=110+(Combo_Vol->ItemIndex); //Combo_Vol->ItemIndex = 25(if 設定電壓=1.35v)
    //ShowMessage( "val="+IntToStr(Val));
    t705=(((Val-100)*2)/10)|0x20;
	//ShowMessage( "t705="+IntToStr(t705));
	Edit_Vol->Text=IntToHex(t705,2);

	if(Combo_VDDQ->ItemIndex==-1){
        StatusB->Panels->Items[0]->Text=("電壓值VDDQ設定錯誤!");
        return;
    }
    Val=110+(Combo_VDDQ->ItemIndex); //Combo_VDDQ->ItemIndex = 25(if 設定電壓=1.35v)
    //ShowMessage( "val="+IntToStr(Val));
    t706=(((Val-100)*2)/10)|0x20;
	//ShowMessage( "t706="+IntToStr(t706));
	Edit_VDDQ->Text=IntToHex(t706,2);

	//-------------------t709 t710(tckavg)-----------------------/
    if(Combo_tCLKAVG->ItemIndex==-1){
        StatusB->Panels->Items[0]->Text=("頻率設定錯誤!");
        return;
    }
    //找出nCycleTime非常重要，因為很多值要靠它計算!
    //先求接近值
    t709=nCycleTime; //求出多少ps
    //ShowMessage( "t709="+IntToStr(t709));
    t710=nCycleTime>>8; //往右位移8bits
	//ShowMessage( "t710="+IntToStr(t710));
    Edit_nCycleTime_MTB->Text=IntToHex((t709&0xff),2);
    Edit_nCycleTime_FTB->Text=IntToHex(t710,2);
	//-------------------t711~t715(tCAS Latency support)----------------------/
    t711=0,t712=0,t713=0,t714=0,t715=0; //init
    if(CheckBox20->Checked)t711=t711|0x1;
    if(CheckBox22->Checked)t711=t711|0x2;
    if(CheckBox24->Checked)t711=t711|0x4;
    if(CheckBox26->Checked)t711=t711|0x8;
    if(CheckBox28->Checked)t711=t711|0x10;
    if(CheckBox30->Checked)t711=t711|0x20;
    if(CheckBox32->Checked)t711=t711|0x40;
    if(CheckBox34->Checked)t711=t711|0x80;
	
    if(CheckBox36->Checked)t712=t712|0x1;
    if(CheckBox38->Checked)t712=t712|0x2;
    if(CheckBox40->Checked)t712=t712|0x4;
    if(CheckBox42->Checked)t712=t712|0x8;
    if(CheckBox44->Checked)t712=t712|0x10;
    if(CheckBox46->Checked)t712=t712|0x20;
    if(CheckBox48->Checked)t712=t712|0x40;
    if(CheckBox50->Checked)t712=t712|0x80;

	
	if(CheckBox52->Checked)t713=t713|0x1;
	if(CheckBox54->Checked)t713=t713|0x2;
	if(CheckBox56->Checked)t713=t713|0x4;
	if(CheckBox58->Checked)t713=t713|0x8;
	if(CheckBox60->Checked)t713=t713|0x10;
	if(CheckBox62->Checked)t713=t713|0x20;
	if(CheckBox64->Checked)t713=t713|0x40;
	if(CheckBox66->Checked)t713=t713|0x80;

    if(CheckBox68->Checked)t714=t714|0x1;
	if(CheckBox70->Checked)t714=t714|0x2;
	if(CheckBox72->Checked)t714=t714|0x4;
	if(CheckBox74->Checked)t714=t714|0x8;
	if(CheckBox76->Checked)t714=t714|0x10;
	if(CheckBox78->Checked)t714=t714|0x20;
	if(CheckBox80->Checked)t714=t714|0x40;
	if(CheckBox82->Checked)t714=t714|0x80;	

	if(CheckBox84->Checked)t715=t715|0x1;
	if(CheckBox86->Checked)t715=t715|0x2;
	if(CheckBox88->Checked)t715=t715|0x4;
	if(CheckBox90->Checked)t715=t715|0x8;
	if(CheckBox92->Checked)t715=t715|0x10;
	if(CheckBox94->Checked)t715=t715|0x20;
	if(CheckBox96->Checked)t715=t715|0x40;
	if(CheckBox98->Checked)t715=t715|0x80;
	
    Edit711->Text=IntToHex((t711&0xff),2);
    Edit712->Text=IntToHex((t712&0xff),2);
    Edit713->Text=IntToHex((t713&0xff),2);
    Edit714->Text=IntToHex((t714&0xff),2);
	Edit715->Text=IntToHex((t715&0xff),2);

	//******716byte要放0x00******

	//-------------------t717 t718(tAAmin)-----------------------/
    if(Combo_tCAS->ItemIndex==-1){
        StatusB->Panels->Items[0]->Text=("tCAS設定錯誤!");
        return;
    }
    t717=(nCycleTime*StrToInt(Combo_tCAS->Text));
    //ShowMessage( "t717="+IntToStr(t717));
    t718=(nCycleTime*StrToInt(Combo_tCAS->Text))>>8;
	//ShowMessage( "t718="+IntToStr(t718));
    Edit_tCAS_MTB->Text=IntToHex((t717&0xff),2);
    Edit_tCAS_FTB->Text=IntToHex(t718,2);

	//-------------------t719 t720(tRCDmin)-----------------------/
    if(Combo_tRCD->ItemIndex==-1){
        StatusB->Panels->Items[0]->Text=("tRCD設定錯誤!");
        return;
    }
    t719=(nCycleTime*StrToInt(Combo_tRCD->Text));
    //ShowMessage( "t719="+IntToStr(t719));
    t720=(nCycleTime*StrToInt(Combo_tRCD->Text))>>8;
	//ShowMessage( "t720="+IntToStr(t720));
    Edit_tRCD_MTB->Text=IntToHex((t719&0xff),2);
    Edit_tRCD_FTB->Text=IntToHex(t720,2);

	//-------------------t721 t722(tRPmin)-----------------------/
    if(Combo_tRP->ItemIndex==-1){
        StatusB->Panels->Items[0]->Text=("tRP設定錯誤!");
        return;
    }
    t721=(nCycleTime*StrToInt(Combo_tRP->Text));
    //ShowMessage( "t721="+IntToStr(t721));
    t722=(nCycleTime*StrToInt(Combo_tRP->Text))>>8;
	//ShowMessage( "t722="+IntToStr(t722));
    Edit_tRP_MTB->Text=IntToHex((t721&0xff),2);
    Edit_tRP_FTB->Text=IntToHex(t722,2);

	//-------------------t723 t724(tRASmin)-----------------------/
    if(Combo_tRAS->ItemIndex==-1){
        StatusB->Panels->Items[0]->Text=("tRAS設定錯誤!");
        return;
    }
    t723=(nCycleTime*StrToInt(Combo_tRAS->Text));
    //ShowMessage( "t723="+IntToStr(t723));
    t724=(nCycleTime*StrToInt(Combo_tRAS->Text))>>8;
	//ShowMessage( "t724="+IntToStr(t724));
    Edit_tRAS_MTB->Text=IntToHex((t723&0xff),2);
    Edit_tRAS_FTB->Text=IntToHex(t724,2);

	//-------------------t725 t726(tRCmin)-----------------------/
    if(Combo_tRC->ItemIndex==-1){
        StatusB->Panels->Items[0]->Text=("tRC設定錯誤!");
        return;
    }
    t725=(nCycleTime*StrToInt(Combo_tRC->Text));
    //ShowMessage( "t725="+IntToStr(t725));
    t726=(nCycleTime*StrToInt(Combo_tRC->Text))>>8;
	//ShowMessage( "t726="+IntToStr(t726));
    Edit_tRC_MTB->Text=IntToHex((t725&0xff),2);
    Edit_tRC_FTB->Text=IntToHex((t726&0xff),2);

	
	//-------------------t727 t728(tWRmin)-----------------------/
    if(Combo_tWR->ItemIndex==-1){
        StatusB->Panels->Items[0]->Text=("tWR設定錯誤!");
        return;
    }
    t727=(nCycleTime*StrToInt(Combo_tWR->Text));
    //ShowMessage( "t727="+IntToStr(t727));
    t728=(nCycleTime*StrToInt(Combo_tWR->Text))>>8;
	//ShowMessage( "t728="+IntToStr(t728));
    Edit_tWR_MTB->Text=IntToHex((t727&0xff),2);
    Edit_tWR_FTB->Text=IntToHex((t728&0xff),2);

	//-------------------t729 t730(tRFC1min)-----------------------/
	int val_t290 = 0;
	if(Edit_tRFC->Text==""){
        StatusB->Panels->Items[0]->Text=("trfc1設定錯誤!");
        return;
    }
	Val = StrToInt(Combo_tCLKAVG->Text);
    val_t290=(StrToInt(Edit_tRFC->Text))*1000/(Val/2);
	t729=val_t290;
    //ShowMessage( "t729="+IntToStr(t729));
    t730=val_t290>>8;
	//ShowMessage( "t730="+IntToStr(t730));
    Edit_tRFC1_LSB->Text=IntToHex((val_t290&0xff),2);
    Edit_tRFC1_MSB->Text=IntToHex((t730&0xff),2);

	//-------------------t731 t732(tRFC2min)-----------------------/
    if(Edit_tRFC2->Text==""){
        StatusB->Panels->Items[0]->Text=("trfc2設定錯誤!");
        return;
    }
	int val_t731 = 0;
    val_t731=(StrToInt(Edit_tRFC2->Text))*1000/(Val/2);
	t731=val_t731;
    //ShowMessage( "t731="+IntToStr(t731));
    t732=val_t731>>8;
	//ShowMessage( "t732="+IntToStr(t732));
    Edit_tRFC2_LSB->Text=IntToHex((val_t731&0xff),2);
    Edit_tRFC2_MSB->Text=IntToHex((t732&0xff),2);

	//-------------------t733 t734(tRFCsbmin)-----------------------/
    if(Edit_tRFCsb->Text==""){
        StatusB->Panels->Items[0]->Text=("trfcsb設定錯誤!");
        return;
    }
	int val_t733 = 0;
    val_t733=(StrToInt(Edit_tRFCsb->Text))*1000/(Val/2);
	t733=val_t733;
    //ShowMessage( "t733="+IntToStr(t733));
    t734=val_t733>>8;
	//ShowMessage( "t734="+IntToStr(t734));
    Edit_tRFCsb_LSB->Text=IntToHex((val_t733&0xff),2);
    Edit_tRFCsb_MSB->Text=IntToHex((t734&0xff),2);
	//--------------------------以上為計算hex值---------------------------/

	//---------------------------開啟檔案---------------------------------/

	 if(USE_HT3310) OpenDialog_F1->InitialDir= GetCurrentDir()+"\\SPD\\";
    else OpenDialog_F1->InitialDir= GetCurrentDir()+"\\";
    OpenDialog_F1->Title    =   "開啟SPD檔";
    OpenDialog_F1->Filter   =   " SPD (*.spd)|*.spd|All files (*.*)|*.*";
    OpenDialog_F1->DefaultExt   =   ".spd";
    OpenDialog_F1->FilterIndex = 1; // start the dialog showing all files
    OpenDialog_F1->FileName="";
    if(OpenDialog_F1->Execute()){
        Lab_Dir->Caption=OpenDialog_F1->FileName;
        OpenDialog_F1->InitialDir="";
    }
    if(Lab_Dir->Caption.IsEmpty()){
        ShowMessage("Please Select file name!");
        return;
    }
    fps = fopen(Lab_Dir->Caption.c_str(), "rt");
    fseek(fps, 0, SEEK_END);//此3行可得到檔案大小.
    int file_size = ftell(fps);
    fseek(fps, 0, SEEK_SET);// 回到檔頭
    //ShowMessage( "Filesize=" + IntToStr(file_size));
	if(file_size == 3200 ){
		 StatusBar1->Panels->Items[0]->Text="SPD檔案大小為3200位元!";
		 NewPrintDataBuffer(Txt_Buf,2048);
	}
	else if(file_size == 3264 ){
		 StatusBar1->Panels->Items[0]->Text="SPD檔案大小為3264位元!";
		 NewPrintDataBuffer(Txt_Buf,2048);
	}
	else {
        StatusBar1->Panels->Items[0]->Text="SPD檔案格式錯誤!";
		//改NewPrintDataBuffer(Txt_Buf,1024);
        NewPrintDataBuffer(Txt_Buf,2048);
        return;
    }
	int len=fread(SPD_Buf, 1, file_size, fps); //從文件中讀取 file_size 個位元組數量的資料到 SPD_Buf 陣列中，並返回實際讀取的位元組數量
    //ShowMessage( IntToStr(len));//1600好奇怪!
	for(int i=0,j=0;i<len;i++){
        if(SPD_Buf[i]!=' ' && SPD_Buf[i]!=0x0d && SPD_Buf[i]!=0x0a){
            Txt_Buf[j++]=SPD_Buf[i];
        }
    }
	//int len=fread(buffer, 1, file_size, fps);
	//for(int i=0,j=0;i<len;i++){
        //if(buffer[i]!=' ' && buffer[i]!=0x0d && buffer[i]!=0x0a){
            //Txt_Buf[j++]=buffer[i];
       // }
   // }
    for(int j=0;j<2048;j=j+2)SPD_Buf[(j/2)]=mstrToByte(Txt_Buf,j); //將文字2048ASCII Code轉成 1024 Bytes (數值)data
    NewPrintDataBuffer(Txt_Buf,2048);
  
	 _bGetColor=true; //===>呼叫StringGridDrawCell()
	int t716 = 0;
	int nCount = 766;
	unsigned short chkcrc = 0;
	//---------------------------------profile1-------------------------------
	if(RadioButton1->Checked==true){
	SPD_Buf[704]=t704;
	SPD_Buf[705]=t705;
    SPD_Buf[706]=t706;
    SPD_Buf[709]=t709;
    SPD_Buf[710]=t710;
    SPD_Buf[711]=t711;
    SPD_Buf[712]=t712;
    SPD_Buf[713]=t713;
    SPD_Buf[714]=t714;
    SPD_Buf[715]=t715;
    SPD_Buf[717]=t717;
    SPD_Buf[718]=t718;
    SPD_Buf[719]=t719;
    SPD_Buf[720]=t720;
    SPD_Buf[721]=t721;
    SPD_Buf[722]=t722;
    SPD_Buf[723]=t723;
    SPD_Buf[724]=t724;
    SPD_Buf[725]=t725;
    SPD_Buf[726]=t726;
    SPD_Buf[727]=t727;
    SPD_Buf[728]=t728;
    SPD_Buf[729]=t729;
    SPD_Buf[730]=t730;
    SPD_Buf[731]=t731;
    SPD_Buf[732]=t732;
	SPD_Buf[733]=t733;
	SPD_Buf[734]=t734;
	//----------------更新buffer_temp1----------------------------//2023/1027
	buffer_temp1[704]=t704;
	buffer_temp1[705]=t705;
    buffer_temp1[706]=t706;
    buffer_temp1[709]=t709;
    buffer_temp1[710]=t710;
    buffer_temp1[711]=t711;
    buffer_temp1[712]=t712;
    buffer_temp1[713]=t713;
    buffer_temp1[714]=t714;
    buffer_temp1[715]=t715;
    buffer_temp1[717]=t717;
    buffer_temp1[718]=t718;
    buffer_temp1[719]=t719;
    buffer_temp1[720]=t720;
    buffer_temp1[721]=t721;
    buffer_temp1[722]=t722;
    buffer_temp1[723]=t723;
    buffer_temp1[724]=t724;
    buffer_temp1[725]=t725;
    buffer_temp1[726]=t726;
    buffer_temp1[727]=t727;
    buffer_temp1[728]=t728;
    buffer_temp1[729]=t729;
    buffer_temp1[730]=t730;
    buffer_temp1[731]=t731;
    buffer_temp1[732]=t732;
	buffer_temp1[733]=t733; 
	buffer_temp1[734]=t734;
	
	/*
	StringGrid1->Cells[0][12]=Edit_VPP->Text.c_str();//t704
    StringGrid1->Cells[1][12]=Edit_Vol->Text.c_str();//t705
    StringGrid1->Cells[2][12]=Edit_VDDQ->Text.c_str();//t706
    StringGrid1->Cells[5][12]=Edit_nCycleTime_MTB->Text.c_str();//t709
    StringGrid1->Cells[6][12]=Edit_nCycleTime_FTB->Text.c_str();//t710
    StringGrid1->Cells[7][12]=Edit711->Text.c_str();//t711
    StringGrid1->Cells[8][12]=Edit712->Text.c_str();//t712
    StringGrid1->Cells[9][12]=Edit713->Text.c_str();//t713
    StringGrid1->Cells[10][12]=Edit714->Text.c_str();//t714
    StringGrid1->Cells[11][12]=Edit715->Text.c_str();//t715
    StringGrid1->Cells[12][12]=IntToHex((t716&0xff),2);//t716預設值0
    StringGrid1->Cells[13][12]=Edit_tCAS_MTB->Text.c_str();//t717
    StringGrid1->Cells[14][12]=Edit_tCAS_FTB->Text.c_str();//t718
    StringGrid1->Cells[15][12]=Edit_tRCD_MTB->Text.c_str();//t719
    StringGrid1->Cells[0][13]=Edit_tRCD_FTB->Text.c_str();//t720
    StringGrid1->Cells[1][13]=Edit_tRP_MTB->Text.c_str();//t721
    StringGrid1->Cells[2][13]=Edit_tRP_FTB->Text.c_str();//t722
    StringGrid1->Cells[3][13]=Edit_tRAS_MTB->Text.c_str();//t723
    StringGrid1->Cells[4][13]=Edit_tRAS_FTB->Text.c_str();//t724
    StringGrid1->Cells[5][13]=Edit_tRC_MTB->Text.c_str();//t725
    StringGrid1->Cells[6][13]=Edit_tRC_FTB->Text.c_str();//t726
    StringGrid1->Cells[7][13]=Edit_tWR_MTB->Text.c_str();//t727
    StringGrid1->Cells[8][13]=Edit_tWR_FTB->Text.c_str();//t728
    StringGrid1->Cells[9][13]=Edit_tRFC1_LSB->Text.c_str();//t729
    StringGrid1->Cells[10][13]=Edit_tRFC1_MSB->Text.c_str();//t730
    StringGrid1->Cells[11][13]=Edit_tRFC2_LSB->Text.c_str();//t731
    StringGrid1->Cells[12][13]=Edit_tRFC2_MSB->Text.c_str();//t732
    StringGrid1->Cells[13][13]=Edit_tRFCsb_LSB->Text.c_str();//t733
    StringGrid1->Cells[14][13]=Edit_tRFCsb_MSB->Text.c_str();//t734
	*/
	//---------------------計算並更新crc---------------------------------------------
	//計算crc
    for (int i=704; i<nCount; i++)chkcrc = mkcrc16(chkcrc, buffer_temp1[i]);
	//ShowMessage(IntToHex(chkcrc,2));
        
    //更新到儲存格
    StringGrid1->Cells[14][15]=IntToHex((chkcrc&0xff),2);//766
    StringGrid1->Cells[15][15]=IntToHex((chkcrc&0xff00)>>8,2);//767

	//更新到buffer_temp1[766][767]
    buffer_temp1[766]= chkcrc&0xff;
    buffer_temp1[767]= (chkcrc&0xff00)>>8;

	//更新到buffer_temp1內的hex轉成文字(char)到Txt_Buf內
	Txt_Buf[766*2]= getnibbleH((char)(buffer_temp1[766] & 0xff));
    Txt_Buf[766*2+1]= getnibbleL((char)(buffer_temp1[766] & 0xff));

	Txt_Buf[767*2]= getnibbleH((char)(buffer_temp1[767] & 0xff));
    Txt_Buf[767*2+1]= getnibbleL((char)(buffer_temp1[767] & 0xff));

	//-----------------------------------------------------------------------------

	
	//更新704~734bytes到Txt_Buf
	for(int i=704;i<=734;i++){
	Txt_Buf[i*2]= getnibbleH((char)(buffer_temp1[i] & 0xff));
    Txt_Buf[i*2+1]= getnibbleL((char)(buffer_temp1[i] & 0xff));
	}

	for(int j=0;j<2048;j=j+2)buffer_temp1[(j/2)]=mstrToByte(Txt_Buf,j); //將文字2048 ASCII Code轉成 1024 Bytes (數值)data
    _bNADimm=false; //Txt_Buf 中的每兩個字元轉換為一個字節，並存儲到 buffer_temp1 陣列中。
    NewPrintDataBuffer(Txt_Buf,2048);
	}
    //---------------------------profile2---------------------------------
	if(RadioButton2->Checked==true){
	SPD_Buf[768]=t704;
	SPD_Buf[769]=t705;
    SPD_Buf[770]=t706;
    SPD_Buf[773]=t709;
    SPD_Buf[774]=t710;
    SPD_Buf[775]=t711;
    SPD_Buf[776]=t712;
    SPD_Buf[777]=t713;
    SPD_Buf[778]=t714;
    SPD_Buf[779]=t715;
    SPD_Buf[781]=t717;
    SPD_Buf[782]=t718;
    SPD_Buf[783]=t719;
    SPD_Buf[784]=t720;
    SPD_Buf[785]=t721;
    SPD_Buf[786]=t722;
    SPD_Buf[787]=t723;
    SPD_Buf[788]=t724;
    SPD_Buf[789]=t725;
    SPD_Buf[790]=t726;
    SPD_Buf[791]=t727;
    SPD_Buf[792]=t728;
    SPD_Buf[793]=t729;
    SPD_Buf[794]=t730;
    SPD_Buf[795]=t731;
    SPD_Buf[796]=t732;
	SPD_Buf[797]=t733;
	SPD_Buf[798]=t734;
	
	buffer_temp1[768]=t704;
	buffer_temp1[769]=t705;
    buffer_temp1[770]=t706;
    buffer_temp1[773]=t709;
    buffer_temp1[774]=t710;
    buffer_temp1[775]=t711;
    buffer_temp1[776]=t712;
    buffer_temp1[777]=t713;
    buffer_temp1[778]=t714;
    buffer_temp1[779]=t715;
    buffer_temp1[781]=t717;
    buffer_temp1[782]=t718;
    buffer_temp1[783]=t719;
    buffer_temp1[784]=t720;
    buffer_temp1[785]=t721;
    buffer_temp1[786]=t722;
    buffer_temp1[787]=t723;
    buffer_temp1[788]=t724;
    buffer_temp1[789]=t725;
    buffer_temp1[790]=t726;
    buffer_temp1[791]=t727;
    buffer_temp1[792]=t728;
    buffer_temp1[793]=t729;
    buffer_temp1[794]=t730;
    buffer_temp1[795]=t731;
    buffer_temp1[796]=t732;
	buffer_temp1[797]=t733;
	buffer_temp1[798]=t734;

	//-----------------------更新CRC-----------------------------
	nCount = 830;
    chkcrc = 0;
    for (int i=768; i<nCount; i++)chkcrc = mkcrc16(chkcrc, buffer_temp1[i]);
    
    //更新到儲存格
    StringGrid1->Cells[14][19]=IntToHex((chkcrc&0xff),2);//830
    StringGrid1->Cells[15][19]=IntToHex((chkcrc&0xff00)>>8,2);//831

	//更新到buffer_temp1[766][767]
    buffer_temp1[830]= chkcrc&0xff;
    buffer_temp1[831]= (chkcrc&0xff00)>>8;

	Txt_Buf[830*2]= getnibbleH((char)(buffer_temp1[830] & 0xff));
    Txt_Buf[830*2+1]= getnibbleL((char)(buffer_temp1[830] & 0xff));

	Txt_Buf[831*2]= getnibbleH((char)(buffer_temp1[831] & 0xff));
    Txt_Buf[831*2+1]= getnibbleL((char)(buffer_temp1[831] & 0xff));
	//------------------------------------------------------------------
	for(int i=768;i<=798;i++){
	Txt_Buf[i*2]= getnibbleH((char)(buffer_temp1[i] & 0xff));
    Txt_Buf[i*2+1]= getnibbleL((char)(buffer_temp1[i] & 0xff));
	}

	for(int j=0;j<2048;j=j+2)SPD_Buf[(j/2)]=mstrToByte(Txt_Buf,j); //將文字2048ASCII Code轉成 1024 Bytes (數值)data
    NewPrintDataBuffer(Txt_Buf,2048);
	}
	//---------------------------profile3---------------------------------
	if(RadioButton3->Checked==true){
	SPD_Buf[832]=t704;
	SPD_Buf[833]=t705;
    SPD_Buf[834]=t706;
    SPD_Buf[837]=t709;
    SPD_Buf[838]=t710;
    SPD_Buf[839]=t711;
    SPD_Buf[840]=t712;
    SPD_Buf[841]=t713;
    SPD_Buf[842]=t714;
    SPD_Buf[843]=t715;
    SPD_Buf[845]=t717;
    SPD_Buf[846]=t718;
    SPD_Buf[847]=t719;
    SPD_Buf[848]=t720;
    SPD_Buf[849]=t721;
    SPD_Buf[850]=t722;
    SPD_Buf[851]=t723;
    SPD_Buf[852]=t724;
    SPD_Buf[853]=t725;
    SPD_Buf[854]=t726;
    SPD_Buf[855]=t727;
    SPD_Buf[856]=t728;
    SPD_Buf[857]=t729;
    SPD_Buf[858]=t730;
    SPD_Buf[859]=t731;
    SPD_Buf[860]=t732;
	SPD_Buf[861]=t733;
	SPD_Buf[862]=t734;
	
	buffer_temp1[832]=t704;
	buffer_temp1[833]=t705;
    buffer_temp1[834]=t706;
    buffer_temp1[837]=t709;
    buffer_temp1[838]=t710;
    buffer_temp1[839]=t711;
    buffer_temp1[840]=t712;
    buffer_temp1[841]=t713;
    buffer_temp1[842]=t714;
    buffer_temp1[843]=t715;
    buffer_temp1[845]=t717;
    buffer_temp1[846]=t718;
    buffer_temp1[847]=t719;
    buffer_temp1[848]=t720;
    buffer_temp1[849]=t721;
    buffer_temp1[850]=t722;
    buffer_temp1[851]=t723;
    buffer_temp1[852]=t724;
    buffer_temp1[853]=t725;
    buffer_temp1[854]=t726;
    buffer_temp1[855]=t727;
    buffer_temp1[856]=t728;
    buffer_temp1[857]=t729;
    buffer_temp1[858]=t730;
    buffer_temp1[859]=t731;
    buffer_temp1[860]=t732;
	buffer_temp1[861]=t733; 
	buffer_temp1[862]=t734;

	//------------------更新CRC--------------------------
	nCount = 894;
    chkcrc = 0;
    for (int i=832; i<nCount; i++)chkcrc = mkcrc16(chkcrc, buffer_temp1[i]);
    
    //更新到儲存格
    StringGrid1->Cells[14][23]=IntToHex((chkcrc&0xff),2);//894
    StringGrid1->Cells[15][23]=IntToHex((chkcrc&0xff00)>>8,2);//895

	//更新到buffer_temp1[766][767]
    buffer_temp1[894]= chkcrc&0xff;
    buffer_temp1[895]= (chkcrc&0xff00)>>8;

	Txt_Buf[894*2]= getnibbleH((char)(buffer_temp1[894] & 0xff));
    Txt_Buf[894*2+1]= getnibbleL((char)(buffer_temp1[894] & 0xff));

	Txt_Buf[895*2]= getnibbleH((char)(buffer_temp1[895] & 0xff));
    Txt_Buf[895*2+1]= getnibbleL((char)(buffer_temp1[895] & 0xff));

	//---------------------------------------------------------------
	for(int i=832;i<=862;i++){
	Txt_Buf[i*2]= getnibbleH((char)(buffer_temp1[i] & 0xff));
    Txt_Buf[i*2+1]= getnibbleL((char)(buffer_temp1[i] & 0xff));
	}

	for(int j=0;j<2048;j=j+2)SPD_Buf[(j/2)]=mstrToByte(Txt_Buf,j); //將文字2048ASCII Code轉成 1024 Bytes (數值)data
    NewPrintDataBuffer(Txt_Buf,2048);
	}
	
    //-------------------------------------------EXPO(842~921)------------------------------------------------------------
    if(EXPOBOX->Checked==true){
	SPD_Buf[842]=t705;
	SPD_Buf[843]=t706;
    SPD_Buf[844]=t704;
    SPD_Buf[846]=t709;
    SPD_Buf[847]=t710;
    SPD_Buf[848]=t717;
    SPD_Buf[849]=t718;
    SPD_Buf[850]=t719;
    SPD_Buf[851]=t720;
    SPD_Buf[852]=t721;
    SPD_Buf[853]=t722;
    SPD_Buf[854]=t723;
    SPD_Buf[855]=t724;
    SPD_Buf[856]=t725;
    SPD_Buf[857]=t726;
    SPD_Buf[858]=t727;
    SPD_Buf[859]=t728;
    SPD_Buf[860]=t729;
    SPD_Buf[861]=t730;
    SPD_Buf[862]=t731;
    SPD_Buf[863]=t732;
    SPD_Buf[864]=t733;
    SPD_Buf[865]=t734;
	
	
	buffer_temp1[842]=t705;
	buffer_temp1[843]=t706;
    buffer_temp1[844]=t704;
    buffer_temp1[846]=t709;
    buffer_temp1[847]=t710;
    buffer_temp1[848]=t717;
    buffer_temp1[849]=t718;
    buffer_temp1[850]=t719;
    buffer_temp1[851]=t720;
    buffer_temp1[852]=t721;
    buffer_temp1[853]=t722;
    buffer_temp1[854]=t723;
    buffer_temp1[855]=t724;
    buffer_temp1[856]=t725;
    buffer_temp1[857]=t726;
    buffer_temp1[858]=t727;
    buffer_temp1[859]=t728;
    buffer_temp1[860]=t729;
    buffer_temp1[861]=t730;
    buffer_temp1[862]=t731;
    buffer_temp1[863]=t732;
    buffer_temp1[864]=t733;
    buffer_temp1[865]=t734;
	

	//------------------更新CRC--------------------------
	nCount = 958;
    chkcrc = 0;
    for (int i=832; i<nCount; i++)chkcrc = mkcrc16(chkcrc, buffer_temp1[i]);
    
    //更新到儲存格
    StringGrid1->Cells[14][27]=IntToHex((chkcrc&0xff),2);//958
    StringGrid1->Cells[15][27]=IntToHex((chkcrc&0xff00)>>8,2);//959

	//更新到buffer_temp1[958][959]
    buffer_temp1[958]= chkcrc&0xff;
    buffer_temp1[959]= (chkcrc&0xff00)>>8;

	Txt_Buf[958*2]= getnibbleH((char)(buffer_temp1[958] & 0xff));
    Txt_Buf[958*2+1]= getnibbleL((char)(buffer_temp1[958] & 0xff));

	Txt_Buf[959*2]= getnibbleH((char)(buffer_temp1[959] & 0xff));
    Txt_Buf[959*2+1]= getnibbleL((char)(buffer_temp1[959] & 0xff));

	//---------------------------------------------------------------
	for(int i=842;i<=865;i++){
	Txt_Buf[i*2]= getnibbleH((char)(buffer_temp1[i] & 0xff));
    Txt_Buf[i*2+1]= getnibbleL((char)(buffer_temp1[i] & 0xff));
	}

	for(int j=0;j<2048;j=j+2)SPD_Buf[(j/2)]=mstrToByte(Txt_Buf,j); //將文字2048ASCII Code轉成 1024 Bytes (數值)data
    NewPrintDataBuffer(Txt_Buf,2048);
	}
	
    
    
    //---------------------------------------------------------------Expo2----------------------------------------------------
    if(EXPOBOX2->Checked==true){
	SPD_Buf[882]=t705;
	SPD_Buf[883]=t706;
    SPD_Buf[884]=t704;
    SPD_Buf[886]=t709;
    SPD_Buf[887]=t710;
    SPD_Buf[888]=t717;
    SPD_Buf[889]=t718;
    SPD_Buf[890]=t719;
    SPD_Buf[891]=t720;
    SPD_Buf[892]=t721;
    SPD_Buf[893]=t722;
    SPD_Buf[894]=t723;
    SPD_Buf[895]=t724;
    SPD_Buf[896]=t725;
    SPD_Buf[897]=t726;
    SPD_Buf[898]=t727;
    SPD_Buf[899]=t728;
    SPD_Buf[900]=t729;
    SPD_Buf[901]=t730;
    SPD_Buf[902]=t731;
    SPD_Buf[903]=t732;
    SPD_Buf[904]=t733;
    SPD_Buf[905]=t734;
	
	
	buffer_temp1[882]=t705;
	buffer_temp1[883]=t706;
    buffer_temp1[884]=t704;
    buffer_temp1[886]=t709;
    buffer_temp1[887]=t710;
    buffer_temp1[888]=t717;
    buffer_temp1[889]=t718;
    buffer_temp1[890]=t719;
    buffer_temp1[891]=t720;
    buffer_temp1[892]=t721;
    buffer_temp1[893]=t722;
    buffer_temp1[894]=t723;
    buffer_temp1[895]=t724;
    buffer_temp1[896]=t725;
    buffer_temp1[897]=t726;
    buffer_temp1[898]=t727;
    buffer_temp1[899]=t728;
    buffer_temp1[900]=t729;
    buffer_temp1[901]=t730;
    buffer_temp1[902]=t731;
    buffer_temp1[903]=t732;
    buffer_temp1[904]=t733;
    buffer_temp1[905]=t734;
	//ShowMessage("expo2改");

	//------------------更新CRC--------------------------
	nCount = 958;
    chkcrc = 0;
    for (int i=832; i<nCount; i++)chkcrc = mkcrc16(chkcrc, buffer_temp1[i]);
    
    //更新到儲存格
    StringGrid1->Cells[14][27]=IntToHex((chkcrc&0xff),2);//958
    StringGrid1->Cells[15][27]=IntToHex((chkcrc&0xff00)>>8,2);//959

	//更新到buffer_temp1[958][959]
    buffer_temp1[958]= chkcrc&0xff;
    buffer_temp1[959]= (chkcrc&0xff00)>>8;

	Txt_Buf[958*2]= getnibbleH((char)(buffer_temp1[958] & 0xff));
    Txt_Buf[958*2+1]= getnibbleL((char)(buffer_temp1[958] & 0xff));

	Txt_Buf[959*2]= getnibbleH((char)(buffer_temp1[959] & 0xff));
    Txt_Buf[959*2+1]= getnibbleL((char)(buffer_temp1[959] & 0xff));

	//---------------------------------------------------------------
	for(int i=882;i<=905;i++){
	Txt_Buf[i*2]= getnibbleH((char)(buffer_temp1[i] & 0xff));
    Txt_Buf[i*2+1]= getnibbleL((char)(buffer_temp1[i] & 0xff));
	}

	for(int j=0;j<2048;j=j+2)SPD_Buf[(j/2)]=mstrToByte(Txt_Buf,j); //將文字2048ASCII Code轉成 1024 Bytes (數值)data
    NewPrintDataBuffer(Txt_Buf,2048);
	}

	//update
    char Setbuf[50];
    memset(WrSPDBuf,0,3264);
    for(int i=0;i<64;i++){
        sprintf(Setbuf," %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X \n",SPD_Buf[i*16+0],SPD_Buf[i*16+1],SPD_Buf[i*16+2],SPD_Buf[i*16+3],SPD_Buf[i*16+4],SPD_Buf[i*16+5],SPD_Buf[i*16+6],SPD_Buf[i*16+7],SPD_Buf[i*16+8],SPD_Buf[i*16+9],SPD_Buf[i*16+10],SPD_Buf[i*16+11],SPD_Buf[i*16+12],SPD_Buf[i*16+13],SPD_Buf[i*16+14],SPD_Buf[i*16+15]);
        memcpy(&WrSPDBuf[i*50],Setbuf,50);
		memset(Setbuf,0,50);
    	}

	fwrite(WrSPDBuf,1,3200,fps); //這行使用 fwrite 函數將 WrSPD_Buf 中的3200個BYTES的數據寫入到檔案指標 fps 所指向的檔案中
    fclose(fps); //關閉檔案指標 fps，確保已完成對該檔案的寫入操作
    
    //顯示記憶體規格
    Form1->LMemSpec->Caption="";
    Form1->Lab_XMP1->Caption="";
    Form1->Lab_XMP3->Caption="";
    Show_MemSpec();
    Show_XMPSpec();
    Show_EXPO();
}
//------------------------------------------------------------------------
void TForm1::NewPrintDataBuffer(PCHAR DataBuffer, ULONG BufferLength)
{
	/*
    ULONG index;
    char printftemp[256];
	for (index=0;index<BufferLength;index=index+2){
        sprintf(printftemp,"%C%C", DataBuffer[index],DataBuffer[index+1]);
        StringGrid2->Cells[((index/2)%16)][((index/2)/16)] = printftemp;
	}
	*/
	ULONG index;
	char printftemp[512];
	int gridSizeX = 16; // 16 列
    int gridSizeY = 16; // 16 行
    for (index = 0; index < BufferLength; index = index + 2) {
        sprintf(printftemp, "%C%C", DataBuffer[index], DataBuffer[index + 1]);

        // 計算列和行
        int gridX = (index / 2) % gridSizeX;
        int gridY = (index / 2) / gridSizeX;

        // 將資料分配到StringGrid1和StringGrid2
        if (gridY < 32) {
            StringGrid2->Cells[gridX][gridY] = printftemp;
        } else {
            StringGrid1->Cells[gridX][gridY - 32] = printftemp;
        }
    }
}

//------------------------------------------------------------------------------
void __fastcall TForm1::StringGrid1DrawCell(TObject *Sender, int ACol,
      int ARow, TRect &Rect, TGridDrawState State)
{
   if(!_bGetColor)return;
   /*
   if((ARow == 0)&& (ACol == 0||ACol == 1)){//manufacture ID
        StringGrid1->Canvas->Brush->Color=clYellow; 
        StringGrid1->Canvas->Font->Color=clBlack;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
    if((ARow == 2)&& (ACol == 10)){//ic die type
        StringGrid1->Canvas->Brush->Color=clLime; 
        StringGrid1->Canvas->Font->Color=clBlack;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
    */
   if(RadioButton1->Checked==true){
	if(ARow == ChRow && ACol == ChCol){
        StringGrid1->Canvas->Brush->Color=clWhite;
        StringGrid1->Canvas->Font->Color=clBlack;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
	if((ARow == 12)&& (ACol == 0)){//t704(Vpp)
        StringGrid1->Canvas->Brush->Color=clYellow; 
        StringGrid1->Canvas->Font->Color=clBlack;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
    if((ARow == 12)&& (ACol == 1||ACol == 2)){//t705,t706(VDD,VDDQ)
        StringGrid1->Canvas->Brush->Color=clMaroon; 
        StringGrid1->Canvas->Font->Color=clWhite;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
    if((ARow == 12)&& (ACol == 5|| ACol == 6)){//t709,T710(tCKAVG)
        StringGrid1->Canvas->Brush->Color=clAqua;
        StringGrid1->Canvas->Font->Color=clBlack;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
    if((ARow == 12)&& (ACol == 7||ACol == 8||ACol == 9||ACol == 10||ACol == 11)){//t711~t715(tCAS SUP)
        StringGrid1->Canvas->Brush->Color=clFuchsia;
        StringGrid1->Canvas->Font->Color=clBlack;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
    if((ARow == 12)&& (ACol == 13||ACol == 14)){//t717,t718(tAA)
        StringGrid1->Canvas->Brush->Color=clMoneyGreen;
        StringGrid1->Canvas->Font->Color=clBlack;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
   if((ARow == 12)&& (ACol == 15)){//t719(tRCD)
        StringGrid1->Canvas->Brush->Color=clSkyBlue;
        StringGrid1->Canvas->Font->Color=clBlack;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
    if((ARow == 13)&& (ACol == 0)){//t720(tRCD)
        StringGrid1->Canvas->Brush->Color=clSkyBlue;
        StringGrid1->Canvas->Font->Color=clBlack;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
    if((ARow == 13)&& (ACol == 1|| ACol == 2)){//t721,t722(tRP)
        StringGrid1->Canvas->Brush->Color=clBlue;
        StringGrid1->Canvas->Font->Color=clWhite;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
    if((ARow == 13)&& (ACol == 3||ACol == 4)){//t723,t724(tRAS)
        StringGrid1->Canvas->Brush->Color=clTeal;
        StringGrid1->Canvas->Font->Color=clWhite;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
   if((ARow == 13)&& (ACol == 5||ACol == 6)){//t725,t726(tRC)
        StringGrid1->Canvas->Brush->Color=clInfoBk;
        StringGrid1->Canvas->Font->Color=clBlack;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
    if((ARow == 13)&& (ACol == 7||ACol == 8)){//t727,t728(tWR)
        StringGrid1->Canvas->Brush->Color=clGreen;
        StringGrid1->Canvas->Font->Color=clWhite;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
    if((ARow == 13)&& (ACol == 9||ACol == 10)){//t729,t730(tRFC1)
        StringGrid1->Canvas->Brush->Color=clMenuHighlight;
        StringGrid1->Canvas->Font->Color=clWhite;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
    if((ARow == 13)&& (ACol == 11||ACol == 12)){//t731,t732(tRFC2)
        StringGrid1->Canvas->Brush->Color=clPurple;
        StringGrid1->Canvas->Font->Color=clWhite;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
    if((ARow == 13)&& (ACol == 13 || ACol == 14)){//t733,T734(tRFCsb)
        StringGrid1->Canvas->Brush->Color=clOlive;
        StringGrid1->Canvas->Font->Color=clWhite;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
    if((ARow == 15)&& (ACol == 14 || ACol == 15)){//t766,T767(crc)
        StringGrid1->Canvas->Brush->Color=clLime;
        StringGrid1->Canvas->Font->Color=clBlack;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
   }

	//-------------------------profile2--------------------------------------
	if(RadioButton2->Checked==true){
	if((ARow == 16)&& (ACol == 0)){//t704(vpp)
        StringGrid1->Canvas->Brush->Color=clYellow; 
        StringGrid1->Canvas->Font->Color=clBlack;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
    if((ARow == 16)&& (ACol == 1||ACol == 2)){//t705,t706(VDD,VDDQ)
        StringGrid1->Canvas->Brush->Color=clMaroon; 
        StringGrid1->Canvas->Font->Color=clWhite;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
    if((ARow == 16)&& (ACol == 5|| ACol == 6)){//t709,T710(tCKAVG)
        StringGrid1->Canvas->Brush->Color=clAqua;
        StringGrid1->Canvas->Font->Color=clBlack;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
    if((ARow == 16)&& (ACol == 7||ACol == 8||ACol == 9||ACol == 10||ACol == 11)){//t711~t715(tCAS SUP)
        StringGrid1->Canvas->Brush->Color=clFuchsia;
        StringGrid1->Canvas->Font->Color=clBlack;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
    if((ARow == 16)&& (ACol == 13||ACol == 14)){//t717,t718(tAA)
        StringGrid1->Canvas->Brush->Color=clMoneyGreen;
        StringGrid1->Canvas->Font->Color=clBlack;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
   if((ARow == 16)&& (ACol == 15)){//t719(tRCD)
        StringGrid1->Canvas->Brush->Color=clSkyBlue;
        StringGrid1->Canvas->Font->Color=clBlack;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
    if((ARow == 17)&& (ACol == 0)){//t720(tRCD)
        StringGrid1->Canvas->Brush->Color=clSkyBlue;
        StringGrid1->Canvas->Font->Color=clBlack;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
    if((ARow == 17)&& (ACol == 1|| ACol == 2)){//t721,t722(tRP)
        StringGrid1->Canvas->Brush->Color=clBlue;
        StringGrid1->Canvas->Font->Color=clWhite;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
    if((ARow == 17)&& (ACol == 3||ACol == 4)){//t723,t724(tRAS)
        StringGrid1->Canvas->Brush->Color=clTeal;
        StringGrid1->Canvas->Font->Color=clWhite;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
   if((ARow == 17)&& (ACol == 5||ACol == 6)){//t725,t726(tRC)
        StringGrid1->Canvas->Brush->Color=clInfoBk;
        StringGrid1->Canvas->Font->Color=clBlack;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
    if((ARow == 17)&& (ACol == 7||ACol == 8)){//t727,t728(tWR)
        StringGrid1->Canvas->Brush->Color=clGreen;
        StringGrid1->Canvas->Font->Color=clWhite;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
    if((ARow == 17)&& (ACol == 9||ACol == 10)){//t729,t730(tRFC1)
        StringGrid1->Canvas->Brush->Color=clMenuHighlight;
        StringGrid1->Canvas->Font->Color=clWhite;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
    if((ARow == 17)&& (ACol == 11||ACol == 12)){//t731,t732(tRFC2)
        StringGrid1->Canvas->Brush->Color=clPurple;
        StringGrid1->Canvas->Font->Color=clWhite;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
    if((ARow == 17)&& (ACol == 13 || ACol == 14)){//t733,T734(tRFCsb)
        StringGrid1->Canvas->Brush->Color=clOlive;
        StringGrid1->Canvas->Font->Color=clWhite;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
      }
    if((ARow == 19)&& (ACol == 14 || ACol == 15)){//t830,T831(crc)
        StringGrid1->Canvas->Brush->Color=clLime;
        StringGrid1->Canvas->Font->Color=clBlack;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
   }

	//---------------------------------profile3-------------------------------------
	if(RadioButton3->Checked==true){
	if((ARow == 20)&& (ACol == 0)){//t704(vpp)
        StringGrid1->Canvas->Brush->Color=clYellow; 
        StringGrid1->Canvas->Font->Color=clBlack;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
    if((ARow == 20)&& (ACol == 1||ACol == 2)){//t705,t706(VDD,VDDQ)
        StringGrid1->Canvas->Brush->Color=clMaroon; 
        StringGrid1->Canvas->Font->Color=clWhite;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
    if((ARow == 20)&& (ACol == 5|| ACol == 6)){//t709,T710(tCKAVG)
        StringGrid1->Canvas->Brush->Color=clAqua;
        StringGrid1->Canvas->Font->Color=clBlack;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
    if((ARow == 20)&& (ACol == 7||ACol == 8||ACol == 9||ACol == 10||ACol == 11)){//t711~t715(tCAS SUP)
        StringGrid1->Canvas->Brush->Color=clFuchsia;
        StringGrid1->Canvas->Font->Color=clBlack;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
    if((ARow == 20)&& (ACol == 13||ACol == 14)){//t717,t718(tAA)
        StringGrid1->Canvas->Brush->Color=clMoneyGreen;
        StringGrid1->Canvas->Font->Color=clBlack;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
   if((ARow == 20)&& (ACol == 15)){//t719(tRCD)
        StringGrid1->Canvas->Brush->Color=clSkyBlue;
        StringGrid1->Canvas->Font->Color=clBlack;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
    if((ARow == 21)&& (ACol == 0)){//t720(tRCD)
        StringGrid1->Canvas->Brush->Color=clSkyBlue;
        StringGrid1->Canvas->Font->Color=clBlack;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
    if((ARow == 21)&& (ACol == 1|| ACol == 2)){//t721,t722(tRP)
        StringGrid1->Canvas->Brush->Color=clBlue;
        StringGrid1->Canvas->Font->Color=clWhite;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
    if((ARow == 21)&& (ACol == 3||ACol == 4)){//t723,t724(tRAS)
        StringGrid1->Canvas->Brush->Color=clTeal;
        StringGrid1->Canvas->Font->Color=clWhite;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
   if((ARow == 21)&& (ACol == 5||ACol == 6)){//t725,t726(tRC)
        StringGrid1->Canvas->Brush->Color=clInfoBk;
        StringGrid1->Canvas->Font->Color=clBlack;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
    if((ARow == 21)&& (ACol == 7||ACol == 8)){//t727,t728(tWR)
        StringGrid1->Canvas->Brush->Color=clGreen;
        StringGrid1->Canvas->Font->Color=clWhite;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
    if((ARow == 21)&& (ACol == 9||ACol == 10)){//t729,t730(tRFC1)
        StringGrid1->Canvas->Brush->Color=clMenuHighlight;
        StringGrid1->Canvas->Font->Color=clWhite;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
    if((ARow == 21)&& (ACol == 11||ACol == 12)){//t731,t732(tRFC2)
        StringGrid1->Canvas->Brush->Color=clPurple;
        StringGrid1->Canvas->Font->Color=clWhite;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
    if((ARow == 21)&& (ACol == 13 || ACol == 14)){//t733,T734(tRFCsb)
        StringGrid1->Canvas->Brush->Color=clOlive;
        StringGrid1->Canvas->Font->Color=clWhite;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
      }
    if((ARow == 23)&& (ACol == 14 || ACol == 15)){//t894,T895(crc)
        StringGrid1->Canvas->Brush->Color=clLime;
        StringGrid1->Canvas->Font->Color=clBlack;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
   }

	//----------------------------------------EXPO----------------------------------------
	if(EXPOBOX->Checked==true){
	if((ARow == 20)&& (ACol == 12)){//t704(vpp)
        StringGrid1->Canvas->Brush->Color=clYellow; 
        StringGrid1->Canvas->Font->Color=clBlack;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
    if((ARow == 20)&& (ACol == 10||ACol == 11)){//t705,t706(VDD,VDDQ)
        StringGrid1->Canvas->Brush->Color=clMaroon; 
        StringGrid1->Canvas->Font->Color=clWhite;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
    if((ARow == 20)&& (ACol == 14|| ACol == 15)){//t709,T710(tCKAVG)
        StringGrid1->Canvas->Brush->Color=clAqua;
        StringGrid1->Canvas->Font->Color=clBlack;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
	/*
    if((ARow == 20)&& (ACol == 7||ACol == 8||ACol == 9||ACol == 10||ACol == 11)){//t711~t715(tCAS SUP)
        StringGrid1->Canvas->Brush->Color=clFuchsia;
        StringGrid1->Canvas->Font->Color=clBlack;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
    */
    if((ARow == 21)&& (ACol == 0||ACol == 1)){//t717,t718(tAA)
        StringGrid1->Canvas->Brush->Color=clMoneyGreen;
        StringGrid1->Canvas->Font->Color=clBlack;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
   if((ARow == 21)&& (ACol == 2)){//t719(tRCD)
        StringGrid1->Canvas->Brush->Color=clSkyBlue;
        StringGrid1->Canvas->Font->Color=clBlack;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
    if((ARow == 21)&& (ACol == 3)){//t720(tRCD)
        StringGrid1->Canvas->Brush->Color=clSkyBlue;
        StringGrid1->Canvas->Font->Color=clBlack;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
    if((ARow == 21)&& (ACol == 4|| ACol == 5)){//t721,t722(tRP)
        StringGrid1->Canvas->Brush->Color=clBlue;
        StringGrid1->Canvas->Font->Color=clWhite;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
    if((ARow == 21)&& (ACol == 6||ACol == 7)){//t723,t724(tRAS)
        StringGrid1->Canvas->Brush->Color=clTeal;
        StringGrid1->Canvas->Font->Color=clWhite;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
   if((ARow == 21)&& (ACol == 8||ACol == 9)){//t725,t726(tRC)
        StringGrid1->Canvas->Brush->Color=clInfoBk;
        StringGrid1->Canvas->Font->Color=clBlack;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
    if((ARow == 21)&& (ACol == 10||ACol == 11)){//t727,t728(tWR)
        StringGrid1->Canvas->Brush->Color=clGreen;
        StringGrid1->Canvas->Font->Color=clWhite;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
    if((ARow == 21)&& (ACol == 12||ACol == 13)){//t729,t730(tRFC1)
        StringGrid1->Canvas->Brush->Color=clMenuHighlight;
        StringGrid1->Canvas->Font->Color=clWhite;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
    if((ARow == 21)&& (ACol == 14||ACol == 15)){//t731,t732(tRFC2)
        StringGrid1->Canvas->Brush->Color=clPurple;
        StringGrid1->Canvas->Font->Color=clWhite;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
    if((ARow == 22)&& (ACol == 0 || ACol == 1)){//t733,T734(tRFCsb)
        StringGrid1->Canvas->Brush->Color=clOlive;
        StringGrid1->Canvas->Font->Color=clWhite;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
      }
     if((ARow == 27)&& (ACol == 14 || ACol == 15)){//t942,T943(crc)
        StringGrid1->Canvas->Brush->Color=clLime;
        StringGrid1->Canvas->Font->Color=clBlack;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
   }
    //----------------------------------------EXPO2----------------------------------------
	if(EXPOBOX2->Checked==true){
	if((ARow == 23)&& (ACol == 4)){//t704(vpp)
        StringGrid1->Canvas->Brush->Color=clYellow; 
        StringGrid1->Canvas->Font->Color=clBlack;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
    if((ARow == 23)&& (ACol == 2||ACol == 3)){//t705,t706(VDD,VDDQ)
        StringGrid1->Canvas->Brush->Color=clMaroon; 
        StringGrid1->Canvas->Font->Color=clWhite;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
    if((ARow == 23)&& (ACol == 6|| ACol == 7)){//t709,T710(tCKAVG)
        StringGrid1->Canvas->Brush->Color=clAqua;
        StringGrid1->Canvas->Font->Color=clBlack;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
	/*
    if((ARow == 20)&& (ACol == 7||ACol == 8||ACol == 9||ACol == 10||ACol == 11)){//t711~t715(tCAS SUP)
        StringGrid1->Canvas->Brush->Color=clFuchsia;
        StringGrid1->Canvas->Font->Color=clBlack;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
    */
    if((ARow == 23)&& (ACol == 8||ACol == 9)){//t717,t718(tAA)
        StringGrid1->Canvas->Brush->Color=clMoneyGreen;
        StringGrid1->Canvas->Font->Color=clBlack;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
   if((ARow == 23)&& (ACol == 10)){//t719(tRCD)
        StringGrid1->Canvas->Brush->Color=clSkyBlue;
        StringGrid1->Canvas->Font->Color=clBlack;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
    if((ARow == 23)&& (ACol == 11)){//t720(tRCD)
        StringGrid1->Canvas->Brush->Color=clSkyBlue;
        StringGrid1->Canvas->Font->Color=clBlack;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
    if((ARow == 23)&& (ACol == 12|| ACol == 13)){//t721,t722(tRP)
        StringGrid1->Canvas->Brush->Color=clBlue;
        StringGrid1->Canvas->Font->Color=clWhite;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
    if((ARow == 23)&& (ACol == 14||ACol == 15)){//t723,t724(tRAS)
        StringGrid1->Canvas->Brush->Color=clTeal;
        StringGrid1->Canvas->Font->Color=clWhite;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
   if((ARow == 24)&& (ACol == 0||ACol == 1)){//t725,t726(tRC)
        StringGrid1->Canvas->Brush->Color=clInfoBk;
        StringGrid1->Canvas->Font->Color=clBlack;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
    if((ARow == 24)&& (ACol == 2||ACol == 3)){//t727,t728(tWR)
        StringGrid1->Canvas->Brush->Color=clGreen;
        StringGrid1->Canvas->Font->Color=clWhite;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
    if((ARow == 24)&& (ACol == 4||ACol == 5)){//t729,t730(tRFC1)
        StringGrid1->Canvas->Brush->Color=clMenuHighlight;
        StringGrid1->Canvas->Font->Color=clWhite;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
    if((ARow == 24)&& (ACol == 6||ACol == 7)){//t731,t732(tRFC2)
        StringGrid1->Canvas->Brush->Color=clPurple;
        StringGrid1->Canvas->Font->Color=clWhite;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
    if((ARow == 24)&& (ACol == 8 || ACol == 9)){//t733,T734(tRFCsb)
        StringGrid1->Canvas->Brush->Color=clOlive;
        StringGrid1->Canvas->Font->Color=clWhite;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
      }
     if((ARow == 27)&& (ACol == 14 || ACol == 15)){//t942,T943(crc)
        StringGrid1->Canvas->Brush->Color=clLime;
        StringGrid1->Canvas->Font->Color=clBlack;
        StringGrid1->Canvas->FillRect(Rect);
        StringGrid1->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid1->Cells[ACol][ARow]);
    }
   }



	
}

void TForm1::ClearGrid(void){
    _bGetColor=false;
    for (int i = 0; i < StringGrid1->RowCount; i++){
        for (int j = 0; j < StringGrid1->ColCount; j++)
        StringGrid1->Cells[j][i] = "";
    }
}


void __fastcall TForm1::Btn_SaveClick(TObject *Sender)
{
    AnsiString FileName;
    AnsiString INIFileName;
    bool Buf_In_Data=false;
    for(int i=0;i<1024;i++){
        if(SPD_Buf[i]!=0){
            Buf_In_Data=true;
            break;
        }
    }
    if(!Buf_In_Data){
        ShowMessage("尚未產生XMP資料，無法存檔!");
        return;
    }
    //---------------------------------------------------------------------
	if(Combo_VPP->ItemIndex==-1){
		ShowMessage("電壓vpp設定值錯誤!");
		return;
	}
	if(Combo_Vol->ItemIndex==-1){
		ShowMessage("電壓設定值錯誤!");
		return;
	}
	if(Combo_VDDQ->ItemIndex==-1){
		ShowMessage("電壓VDDQ設定值錯誤!");
		return;
	}
	if(Combo_tCLKAVG->ItemIndex==-1){
		ShowMessage("頻率設定值錯誤!");
		return;
	}
	if(Combo_tCAS->ItemIndex==-1){
		ShowMessage("tCAS設定值錯誤!");
		return;
	}
	if(Combo_tRCD->ItemIndex==-1){
		ShowMessage("tRCD設定值錯誤!");
		return;
	}
	if(Combo_tRP->ItemIndex==-1){
		ShowMessage("tRP設定值錯誤!");
		return;
	}
	if(Combo_tRAS->ItemIndex==-1){
		ShowMessage("tRAS設定值錯誤!");
		return;
	}
	if(Combo_tRC->ItemIndex==-1){
		ShowMessage("tRC設定值錯誤!");
		return;
	}
	if(Edit_tRFC->Text.IsEmpty()){
		ShowMessage("tRFC1設定值錯誤!");
		return;
	}
	if(Edit_tRFC2->Text.IsEmpty()){
		ShowMessage("tRFC2設定值錯誤!");
		return;
	}
	if(Edit_tRFCsb->Text.IsEmpty()){
		ShowMessage("tRFCsb設定值錯誤!");
		return;
	}
	if(Combo_tWR->ItemIndex==-1){
		ShowMessage("tWR設定值錯誤!");
		return;
	}
    //---------------------------------------------------------------------
     if(RadioBIN->Checked==true){
        if(USE_HT3310)SaveDialog_F1->InitialDir= GetCurrentDir()+"\\Bin\\";
        else SaveDialog_F1->InitialDir= GetCurrentDir()+"\\";
        SaveDialog_F1->Title    =   "儲存BIN檔";
        SaveDialog_F1->Filter   =   "BIN   files   (*.bin)|*.bin" ;
        SaveDialog_F1->DefaultExt   =   ".bin";
   	    SaveDialog_F1->FilterIndex = 1; // start the dialog showing all files
        SaveDialog_F1->FileName=Combo_tCLKAVG->Text+"MHz"+Combo_tCAS->Text+"-"+Combo_tRCD->Text+"-"+Combo_tRP->Text+"-"+Combo_tRAS->Text+"_tRFC"+Edit_tRFC->Text+"_"+Combo_Vol->Text+"V.bin";
        if(SaveDialog_F1->Execute()){
            FileName=SaveDialog_F1->FileName;
            //INIFileName=FileName.SubString(0,FileName.Length()-3);
            INIFileName=SaveDialog_F1->FileName;
            INIFileName=StringReplace(INIFileName, "bin", "ini", TReplaceFlags()<<rfReplaceAll);
            INIFileName=StringReplace(INIFileName, "Bin", "Ini", TReplaceFlags()<<rfReplaceAll);
            //ShowMessage(INIFileName);
            //SaveDialog_F1->InitialDir="";
        }
        if(FileName=="")return;  /******按取消,不存檔*******/
        if(FileExists(FileName)){
            char idYesNo;
            idYesNo=MessageBox(NULL,"檔案已經存在，是否覆蓋?","Caution!",MB_YESNO+64);
            if(idYesNo==IDNO)return;
        }
        FILE *fps = fopen((FileName).c_str(), "wb");
        if (fps == NULL) {
            MessageBox ( NULL,(FileName.c_str()),"Error", MB_ICONSTOP );
            fclose(fps);
            return;
        }
        fwrite(SPD_Buf,1,1024,fps);
        fclose(fps);
        Beep(2000,200);
    }else{
        if(USE_HT3310)SaveDialog_F1->InitialDir= GetCurrentDir()+"\\SPD\\";
        else SaveDialog_F1->InitialDir= GetCurrentDir()+"\\";
        SaveDialog_F1->Title    =   "儲存SPD檔";
        SaveDialog_F1->Filter   =   "SPD   files   (*.spd)|*.spd" ;
        SaveDialog_F1->DefaultExt   =   ".spd";
   	    SaveDialog_F1->FilterIndex = 1; // start the dialog showing all files
        SaveDialog_F1->FileName=Combo_tCLKAVG->Text+"MHz"+Combo_tCAS->Text+"-"+Combo_tRCD->Text+"-"+Combo_tRP->Text+"-"+Combo_tRAS->Text+"_tRFC"+Edit_tRFC->Text+"_"+Combo_Vol->Text+"V.spd";
        if(SaveDialog_F1->Execute()){
            FileName=SaveDialog_F1->FileName;
            INIFileName=SaveDialog_F1->FileName;
            INIFileName=StringReplace(INIFileName, "spd", "ini", TReplaceFlags()<<rfReplaceAll);
            INIFileName=StringReplace(INIFileName, "SPD", "Ini", TReplaceFlags()<<rfReplaceAll);
            SaveDialog_F1->InitialDir="";
        }
        if(FileName=="")return;  /******按取消,不存檔*******/
        SaveDialog_F1->FileName="";
        if(FileExists(FileName)){
            char idYesNo;
            idYesNo=MessageBox(NULL,"檔案已經存在，是否覆蓋?","Caution!",MB_YESNO+64);
            if(idYesNo==IDNO)return;
        }
        FILE *fps = fopen((FileName).c_str(), "wt");
        if (fps == NULL) {
            MessageBox ( NULL,(FileName.c_str()),"Error", MB_ICONSTOP );
            fclose(fps);
            return;
        }
        
		char Setbuf[60]; 
		int spaceCount = 0;
   	 	memset(SPD_Buf,0,3264); //先將存檔後的3264位元的spd_buf設為0,所以不包含任何垃圾符號
    	for(int i=0;i<64;i++)
		{ //執行64次,將一個格式化的字串寫入 Setbuf 中。這個字串包括了16個HEX,中間有空格,最後有換行符,迭代總共3264位元 
        sprintf(Setbuf," %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X \n",buffer_temp1[i*16+0],buffer_temp1[i*16+1],buffer_temp1[i*16+2],buffer_temp1[i*16+3],buffer_temp1[i*16+4],buffer_temp1[i*16+5],buffer_temp1[i*16+6],buffer_temp1[i*16+7],buffer_temp1[i*16+8],buffer_temp1[i*16+9],buffer_temp1[i*16+10],buffer_temp1[i*16+11],buffer_temp1[i*16+12],buffer_temp1[i*16+13],buffer_temp1[i*16+14],buffer_temp1[i*16+15]); 
		memcpy(&SPD_Buf[i*50],Setbuf,50); //使用 memcpy 函數將 Setbuf 中的50個字符復製到 SPD_Buf 中 (50個BYTES代表16組HEX+空白鍵+換行)
		memset(Setbuf,0,50);
    	} //---->一行50BYTES*64列 +    每一行空白 = 3264BYTES
	
		fwrite(SPD_Buf,1,3200,fps); //這行使用 fwrite 函數將 SPD_Buf 中的3200個BYTES的數據寫入到檔案指標 fps 所指向的檔案中
    	fclose(fps); //關閉檔案指標 fps，確保已完成對該檔案的寫入操作
		ShowMessage("123");
        Beep(2000,200);
    }
    if(USE_HT3310){
		if(INIFileName=="")return;  /******按取消,不存檔*******/
		SaveDialog_F1->FileName="";
		FILE *INIfps = fopen((INIFileName).c_str(), "wt");
		if (INIfps == NULL) {
			MessageBox ( NULL,(INIFileName.c_str()),"Error", MB_ICONSTOP );
			fclose(INIfps);
			return;
		}
		char SetBuf[2];
		SetBuf[0]=0x0d;
		SetBuf[0]=0x0a;
		fwrite(Combo_VPP->Text.c_str(),1,Combo_VPP->Text.Length(),INIfps);
		fwrite(SetBuf,1,1,INIfps);
		fwrite(Combo_Vol->Text.c_str(),1,Combo_Vol->Text.Length(),INIfps);
		fwrite(SetBuf,1,1,INIfps);
		fwrite(Combo_VDDQ->Text.c_str(),1,Combo_VDDQ->Text.Length(),INIfps);
		fwrite(SetBuf,1,1,INIfps);
		fwrite(Combo_tCLKAVG->Text.c_str(),1,Combo_tCLKAVG->Text.Length(),INIfps);
		fwrite(SetBuf,1,1,INIfps);
		fwrite(Combo_tCAS->Text.c_str(),1,Combo_tCAS->Text.Length(),INIfps);
		fwrite(SetBuf,1,1,INIfps);
		fwrite(Combo_tRCD->Text.c_str(),1,Combo_tRCD->Text.Length(),INIfps);
		fwrite(SetBuf,1,1,INIfps);
		fwrite(Combo_tRP->Text.c_str(),1,Combo_tRP->Text.Length(),INIfps);
		fwrite(SetBuf,1,1,INIfps);
		fwrite(Combo_tRAS->Text.c_str(),1,Combo_tRAS->Text.Length(),INIfps);
		fwrite(SetBuf,1,1,INIfps);
		fwrite(Combo_tRC->Text.c_str(),1,Combo_tRC->Text.Length(),INIfps);
		fwrite(SetBuf,1,1,INIfps);
		fwrite(Combo_tWR->Text.c_str(),1,Combo_tWR->Text.Length(),INIfps);
		fwrite(SetBuf,1,1,INIfps);
		fwrite(Edit_tRFC->Text.c_str(),1,Edit_tRFC->Text.Length(),INIfps);
		fwrite(SetBuf,1,1,INIfps);
		fwrite(Edit_tRFC2->Text.c_str(),1,Edit_tRFC2->Text.Length(),INIfps);
		fwrite(SetBuf,1,1,INIfps);
		fwrite(Edit_tRFCsb->Text.c_str(),1,Edit_tRFCsb->Text.Length(),INIfps);
		fwrite(SetBuf,1,1,INIfps);
		fclose(INIfps);
        Beep(2000,200);
		//ShowMessage("456");
    }
}


void __fastcall TForm1::Lb_MiniMouseDown(TObject *Sender,
      TMouseButton Button, TShiftState Shift, int X, int Y)
{
    if(Button == mbLeft){
        this->Img_minimize->Picture->Graphic=this->Img_mini2->Picture->Graphic;
    }  
}
//---------------------------------------------------------------------------

void __fastcall TForm1::Lb_CloseMouseDown(TObject *Sender,
      TMouseButton Button, TShiftState Shift, int X, int Y)
{
    if(Button == mbLeft){
        this->Image_Close->Picture->Graphic=this->Image_Close_Down->Picture->Graphic;
    }     
}
//---------------------------------------------------------------------------

void __fastcall TForm1::Lb_CloseMouseEnter(TObject *Sender)
{
    this->Image_Close->Picture->Graphic=this->Image_Close_Over->Picture->Graphic;    
}
//---------------------------------------------------------------------------

void __fastcall TForm1::Lb_CloseMouseLeave(TObject *Sender)
{
    this->Image_Close->Picture->Graphic=this->Image_Close_Normal->Picture->Graphic;    
}
//---------------------------------------------------------------------------

void __fastcall TForm1::Lb_CloseMouseUp(TObject *Sender,
      TMouseButton Button, TShiftState Shift, int X, int Y)
{
    if(Button == mbLeft){
        this->Image_Close->Picture->Graphic=this->Image_Close_Over->Picture->Graphic;
        TPoint p = Mouse->CursorPos;
        Form1->ScreenToClient(p);
        //ShowMessage(IntToStr((int)p.x)+"/"+IntToStr((int)p.y)); //for Debug
        //if(((int)p.x<(this->Left+this->Width-Lb_Close->Width)) || ((int)p.x>(this->Left+this->Width)) || ((int)p.y<this->Top) || ((int)p.y>(this->Top+Lb_Close->Height)))return;
        this->Close();
    }    
}
//---------------------------------------------------------------------------

void __fastcall TForm1::Lb_MiniMouseEnter(TObject *Sender)
{
    this->Img_minimize->Picture->Graphic=this->Img_mini1->Picture->Graphic;    
}
//---------------------------------------------------------------------------

void __fastcall TForm1::Lb_MiniMouseLeave(TObject *Sender)
{
    this->Img_minimize->Picture->Graphic=this->Img_mini5->Picture->Graphic;    
}
//---------------------------------------------------------------------------

void __fastcall TForm1::Lb_MiniMouseUp(TObject *Sender,
      TMouseButton Button, TShiftState Shift, int X, int Y)
{
    if(Button == mbLeft){
        this->Img_minimize->Picture->Graphic=this->Img_mini5->Picture->Graphic;
        TPoint p = Mouse->CursorPos;
        this->ScreenToClient(p);
        //ShowMessage(IntToStr((int)p.x)+"/"+IntToStr((int)p.y)); //for Debug
        //if(((int)p.x<this->Left) || ((int)p.x>(this->Left+Lb_Mini->Width)) || ((int)p.y<this->Top) || ((int)p.y>(this->Top+Lb_Mini->Height)))return;
        Application->Minimize();
    }     
}
//---------------------------------------------------------------------------

void __fastcall TForm1::FormMouseDown(TObject *Sender, TMouseButton Button,
      TShiftState Shift, int X, int Y)
{
        if(Button == mbLeft)
        {
                ReleaseCapture();
                Perform(WM_SYSCOMMAND, SC_MOVE+HTCAPTION,0);
                Application->ProcessMessages();

        }    
}
//--------------------------------------------------------------------------
void __fastcall TForm1::Btn_SaveiniClick(TObject *Sender)
{
	if(Combo_VPP->ItemIndex==-1){
		ShowMessage("電壓vpp設定值錯誤!");
		return;
	}
	if(Combo_Vol->ItemIndex==-1){
		ShowMessage("電壓設定值錯誤!");
		return;
	}
	if(Combo_VDDQ->ItemIndex==-1){
		ShowMessage("電壓vDDQ設定值錯誤!");
		return;
	}
	if(Combo_tCLKAVG->ItemIndex==-1){
		ShowMessage("頻率設定值錯誤!");
		return;
	}
	if(Combo_tCAS->ItemIndex==-1){
		ShowMessage("tCAS設定值錯誤!");
		return;
	}
	if(Combo_tRCD->ItemIndex==-1){
		ShowMessage("tRCD設定值錯誤!");
		return;
	}
	if(Combo_tRP->ItemIndex==-1){
		ShowMessage("tRP設定值錯誤!");
		return;
	}
	if(Combo_tRAS->ItemIndex==-1){
		ShowMessage("tRAS設定值錯誤!");
		return;
	}
	if(Combo_tRC->ItemIndex==-1){
		ShowMessage("tRC設定值錯誤!");
		return;
	}
	if(Edit_tRFC->Text.IsEmpty()){
		ShowMessage("tRFC1設定值錯誤!");
		return;
	}
	if(Edit_tRFC2->Text.IsEmpty()){
		ShowMessage("tRFC2設定值錯誤!");
		return;
	}
	if(Edit_tRFCsb->Text.IsEmpty()){
		ShowMessage("tRFCsb設定值錯誤!");
		return;
	}
	if(Combo_tWR->ItemIndex==-1){
		ShowMessage("tWR設定值錯誤!");
		return;
	}
    AnsiString INIFileName;
    if(USE_HT3310)SaveDialog_F1->InitialDir= GetCurrentDir()+"\\Ini\\";
    else SaveDialog_F1->InitialDir= GetCurrentDir()+"\\";
    SaveDialog_F1->Title    =   "儲存INI檔";
    SaveDialog_F1->Filter   =   "INI   files   (*.ini)|*.ini" ;
    SaveDialog_F1->DefaultExt   =   ".ini";
    SaveDialog_F1->FilterIndex = 1; // start the dialog showing all files
    SaveDialog_F1->FileName=Combo_tCLKAVG->Text+"MHz"+Combo_tCAS->Text+"-"+Combo_tRCD->Text+"-"+Combo_tRP->Text+"-"+Combo_tRAS->Text+"_tRFC"+Edit_tRFC->Text+"_"+Combo_Vol->Text+"V.ini";
    if(SaveDialog_F1->Execute()){
        INIFileName=SaveDialog_F1->FileName;
        SaveDialog_F1->InitialDir="";
    }
    if(INIFileName=="")return;  /******按取消,不存檔*******/
    SaveDialog_F1->FileName="";
	if(FileExists(INIFileName)){
		char idYesNo;
		idYesNo=MessageBox(NULL,"檔案已經存在，是否覆蓋?","Caution!",MB_YESNO+64);
		if(idYesNo==IDNO)return;
	}
    FILE *INIfps = fopen((INIFileName).c_str(), "wt");
    if (INIfps == NULL) {
        MessageBox ( NULL,(INIFileName.c_str()),"Error", MB_ICONSTOP );
        fclose(INIfps);
        return;
    }
    char SetBuf[2];
    SetBuf[0]=0x0d;
    SetBuf[0]=0x0a;
	fwrite(Combo_VPP->Text.c_str(),1,Combo_VPP->Text.Length(),INIfps);
    fwrite(SetBuf,1,1,INIfps);
    fwrite(Combo_Vol->Text.c_str(),1,Combo_Vol->Text.Length(),INIfps);
    fwrite(SetBuf,1,1,INIfps);
	fwrite(Combo_VDDQ->Text.c_str(),1,Combo_VDDQ->Text.Length(),INIfps);
    fwrite(SetBuf,1,1,INIfps);
    fwrite(Combo_tCLKAVG->Text.c_str(),1,Combo_tCLKAVG->Text.Length(),INIfps);
    fwrite(SetBuf,1,1,INIfps);
    fwrite(Combo_tCAS->Text.c_str(),1,Combo_tCAS->Text.Length(),INIfps);
    fwrite(SetBuf,1,1,INIfps);
    fwrite(Combo_tRCD->Text.c_str(),1,Combo_tRCD->Text.Length(),INIfps);
    fwrite(SetBuf,1,1,INIfps);
    fwrite(Combo_tRP->Text.c_str(),1,Combo_tRP->Text.Length(),INIfps);
    fwrite(SetBuf,1,1,INIfps);
    fwrite(Combo_tRAS->Text.c_str(),1,Combo_tRAS->Text.Length(),INIfps);
    fwrite(SetBuf,1,1,INIfps);
    fwrite(Combo_tRC->Text.c_str(),1,Combo_tRC->Text.Length(),INIfps);
    fwrite(SetBuf,1,1,INIfps);
	fwrite(Combo_tWR->Text.c_str(),1,Combo_tWR->Text.Length(),INIfps);
    fwrite(SetBuf,1,1,INIfps);
	fwrite(Edit_tRFC->Text.c_str(),1,Edit_tRFC->Text.Length(),INIfps);
    fwrite(SetBuf,1,1,INIfps);
    fwrite(Edit_tRFC2->Text.c_str(),1,Edit_tRFC2->Text.Length(),INIfps);
    fwrite(SetBuf,1,1,INIfps);
    fwrite(Edit_tRFCsb->Text.c_str(),1,Edit_tRFCsb->Text.Length(),INIfps);
    fwrite(SetBuf,1,1,INIfps);
    fclose(INIfps);
    Beep(2000,400);
    StatusB->SimpleText=("ini Save Done!");
}
//---------------------------------------------------------------------------------

void __fastcall TForm1::Btn_LoadiniClick(TObject *Sender)
{
	int line_cnt=0;
	char Buffer[32]={0};
    AnsiString INIFileName;
    OpenDialog_F1->InitialDir= GetCurrentDir()+"\\";
    OpenDialog_F1->Title    =   "開啟INI檔";
    OpenDialog_F1->Filter   =   " INI (*.ini)|*.ini|All files (*.*)|*.*";
    OpenDialog_F1->DefaultExt   =   ".ini";
    OpenDialog_F1->FilterIndex = 1; // start the dialog showing all files
    if(OpenDialog_F1->Execute()){
        INIFileName=OpenDialog_F1->FileName;
        OpenDialog_F1->InitialDir="";
    }
    if(INIFileName.IsEmpty()){
        ShowMessage("Please Select file name!");
        return;
    }
    if(INIFileName=="")return;  /******按取消,不存檔*******/
    FILE *INIfps = fopen((INIFileName).c_str(), "rt");
    if (INIfps == NULL) {
        MessageBox ( NULL,(INIFileName.c_str()),"Error", MB_ICONSTOP );
        fclose(INIfps);
        return;
    }
	//獲取檔案行數
    fseek(INIfps, 0, SEEK_SET); // 回到檔頭
	while(fgets(Buffer, 32, INIfps)!=NULL) ++line_cnt;
    fseek(INIfps, 0, SEEK_END);//得到檔案大小.
    int file_size = ftell(INIfps);
    fseek(INIfps, 0, SEEK_SET); // 回到檔頭
    //if(file_size!=55){
        //ShowMessage("ini 檔案格式錯誤!");
		//ShowMessage("filesize="+ IntToStr(file_size));
       // return;
   // }
	//ShowMessage("filesize="+ IntToStr(file_size));
    int k;
	fgets(Buffer, 32, INIfps);
    Combo_VPP->Text=Trim((AnsiString)Buffer);
    if(Combo_VPP->Items->IndexOf(Combo_VPP->Text)!=-1){
        Combo_VPP->ItemIndex=Combo_VPP->Items->IndexOf(Combo_VPP->Text);
    }else{
        StatusB->Panels->Items[1]->Text = "電壓vpp設定值錯誤";
        return;
    }


	
	memset(Buffer,0,32);
    fgets(Buffer, 32, INIfps);
    Combo_Vol->Text=Trim((AnsiString)Buffer);
    if(Combo_Vol->Items->IndexOf(Combo_Vol->Text)!=-1){
        Combo_Vol->ItemIndex=Combo_Vol->Items->IndexOf(Combo_Vol->Text);
    }else{
        StatusB->Panels->Items[1]->Text = "電壓設定值錯誤";
        return;
    }
	
	memset(Buffer,0,32);
    fgets(Buffer, 32, INIfps);
    Combo_VDDQ->Text=Trim((AnsiString)Buffer);
	//ShowMessage("Buffer="+ (AnsiString)Buffer);
    if(Combo_VDDQ->Items->IndexOf(Combo_VDDQ->Text)!=-1){
        Combo_VDDQ->ItemIndex=Combo_VDDQ->Items->IndexOf(Combo_VDDQ->Text);
    }else{
        StatusB->Panels->Items[1]->Text = "電壓VDDQ設定值錯誤123";
        return;
    }
	
    memset(Buffer,0,32);
    fgets(Buffer, 32, INIfps);
    Combo_tCLKAVG->Text=Trim((AnsiString)Buffer);
    if(Combo_tCLKAVG->Items->IndexOf(Combo_tCLKAVG->Text)!=-1){
        Combo_tCLKAVG->ItemIndex=Combo_tCLKAVG->Items->IndexOf(Combo_tCLKAVG->Text);
    }else{
        StatusB->Panels->Items[1]->Text = "頻率設定值錯誤";
        return;
    }
	
    memset(Buffer,0,32);
    fgets(Buffer, 32, INIfps);
    Combo_tCAS->Text=Trim((AnsiString)Buffer);
    if(Combo_tCAS->Items->IndexOf(Combo_tCAS->Text)!=-1){
        Combo_tCAS->ItemIndex=Combo_tCAS->Items->IndexOf(Combo_tCAS->Text);
    }else{
        StatusB->Panels->Items[1]->Text = "tCAS設定值錯誤";
        return;
    }
	
    memset(Buffer,0,32);
    fgets(Buffer, 32, INIfps);
    Combo_tRCD->Text=Trim((AnsiString)Buffer);
    if(Combo_tRCD->Items->IndexOf(Combo_tRCD->Text)!=-1){
        Combo_tRCD->ItemIndex=Combo_tRCD->Items->IndexOf(Combo_tRCD->Text);
    }else{
        StatusB->Panels->Items[1]->Text = "tRCD設定值錯誤";
        return;
    }
	
    memset(Buffer,0,32);
    fgets(Buffer, 32, INIfps);
    Combo_tRP->Text=Trim((AnsiString)Buffer);
    if(Combo_tRP->Items->IndexOf(Combo_tRP->Text)!=-1){
        Combo_tRP->ItemIndex=Combo_tRP->Items->IndexOf(Combo_tRP->Text);
    }else{
        StatusB->Panels->Items[1]->Text = "tRP設定值錯誤";
        return;
    }
	
    memset(Buffer,0,32);
    fgets(Buffer, 32, INIfps);
    Combo_tRAS->Text=Trim((AnsiString)Buffer);
    if(Combo_tRAS->Items->IndexOf(Combo_tRAS->Text)!=-1){
        Combo_tRAS->ItemIndex=Combo_tRAS->Items->IndexOf(Combo_tRAS->Text);
    }else{
        StatusB->Panels->Items[1]->Text = "tRAS設定值錯誤";
        return;
    }
	
    memset(Buffer,0,32);
    fgets(Buffer, 32, INIfps);
    Combo_tRC->Text=Trim((AnsiString)Buffer);
    if(Combo_tRC->Items->IndexOf(Combo_tRC->Text)!=-1){
        Combo_tRC->ItemIndex=Combo_tRC->Items->IndexOf(Combo_tRC->Text);
    }else{
        StatusB->Panels->Items[1]->Text = "tRC設定值錯誤";
        return;
    }

	memset(Buffer,0,32);
    fgets(Buffer, 32, INIfps);
    Combo_tWR->Text=Trim((AnsiString)Buffer);
    if(Combo_tWR->Items->IndexOf(Combo_tWR->Text)!=-1){
        Combo_tWR->ItemIndex=Combo_tWR->Items->IndexOf(Combo_tWR->Text);
    }else{
        StatusB->Panels->Items[1]->Text = "twr設定值錯誤123";
        return;
    }
	
    memset(Buffer,0,32);
    fgets(Buffer, 32, INIfps);
    Edit_tRFC->Text=Trim((AnsiString)Buffer);

	memset(Buffer,0,32);
    fgets(Buffer, 32, INIfps);
    Edit_tRFC2->Text=Trim((AnsiString)Buffer);

	memset(Buffer,0,32);
    fgets(Buffer, 32, INIfps);
    Edit_tRFCsb->Text=Trim((AnsiString)Buffer);
	
    
    memset(Buffer,0,32);
    fclose(INIfps);
    Beep(1000,200);
}
//---------------------------------------------------------------------------


void __fastcall TForm1::BitBtn1Click(TObject *Sender) //開啟spd檔
{
	AnsiString SPD_name;
    long file_size;
    int totalByte;
    FILE *fps;
	char Buffer[32]={0};
    memset(SPD_Buf,0x0,2048); //將記憶體設定為零，確保陣列是空的且可以準備好被使用
    memset(Txt_Buf,0x0,1088); //將記憶體設定為零，確保陣列是空的且可以準備好被使用
    OpenDialog1->InitialDir= GetCurrentDir()+"\\";
    OpenDialog1->Title    =   "開啟SPD檔";
    OpenDialog1->Filter   =   " SPD (*.spd)|*.spd|All files (*.*)|*.*";
    OpenDialog1->DefaultExt   =   ".spd";
    OpenDialog1->FilterIndex = 1; // start the dialog showing all files
    OpenDialog1->FileName="";
    if(OpenDialog1->Execute()){
        SPD_name=OpenDialog1->FileName;
        OpenDialog1->InitialDir="";
    }
    if(SPD_name.IsEmpty()){
        ShowMessage("Please Select file name!");
        return;
    }
    fps = fopen(SPD_name.c_str(), "rt"); //spdname=檔案來源
    if(!FileExists(SPD_name.c_str())){
        StatusBar1->Panels->Items[0]->Text =  SPD_name + " :not found!";
        NewPrintDataBuffer(Txt_Buf,2048);
        return;
    }
    if (fps == NULL) {
        StatusBar1->Panels->Items[0]->Text = "SPD file : "+SPD_name+" open failed.";
        fclose(fps);
        NewPrintDataBuffer(Txt_Buf,2048);
        return;
    }

    fseek(fps, 0, SEEK_END);//此3行可得到檔案大小.
    file_size = ftell(fps); //filesize=1632
    fseek(fps, 0, SEEK_SET);

	//fgets(Buffer, 32, fps);
    //Combo_Vol->Text=Trim((AnsiString)Buffer);
	//ShowMessage((AnsiString)Buffer);
	//ShowMessage((AnsiString)file_size);
    //ShowMessage("filesize="+IntToStr(file_size)); //for Debug
    /*改成獲取有效行列數來判斷spd file的格式*/
    if(file_size>4096){
        StatusBar1->Panels->Items[0]->Text="檔案過大,Data Buffer配置不足!";
		//改NewPrintDataBuffer(Txt_Buf,1024);
        NewPrintDataBuffer(Txt_Buf,2048);
        return;
    }
	else if(file_size == 3200 ){
		 StatusBar1->Panels->Items[0]->Text="SPD檔案大小為3200位元!";
		 NewPrintDataBuffer(Txt_Buf,2048);
	}
	else if(file_size == 3264 ){
		 StatusBar1->Panels->Items[0]->Text="SPD檔案大小為3264位元!";
		 NewPrintDataBuffer(Txt_Buf,2048);
	}
	else {
        StatusBar1->Panels->Items[0]->Text="SPD檔案格式錯誤!";
		//改NewPrintDataBuffer(Txt_Buf,1024);
        NewPrintDataBuffer(Txt_Buf,2048);
        return;
    }

    int len=fread(SPD_Buf, 1, file_size, fps); //從文件中讀取 file_size 個位元組數量的資料到 SPD_Buf 陣列中，並返回實際讀取的位元組數量
    //ShowMessage( "Read file size:"+IntToStr(len));//1632bytes
    for(int i=0,j=0;i<len;i++){
        if(SPD_Buf[i]!=' ' && SPD_Buf[i]!=0x0d && SPD_Buf[i]!=0x0a){ //迴圈遍歷 SPD_Buf 陣列中的每個元素，將不是空格、0x0d（回車符）和 0x0a（換行符）的字符存儲到 Txt_Buf 陣列中
            Txt_Buf[j++]=SPD_Buf[i]; 
        }
    }
    for(int j=0;j<2048;j=j+2)buffer_temp1[(j/2)]=mstrToByte(Txt_Buf,j); //將文字2048 ASCII Code轉成 1024 Bytes (數值)data
    _bNADimm=false; //Txt_Buf 中的每兩個字元轉換為一個字節，並存儲到 buffer_temp1 陣列中。
    NewPrintDataBuffer(Txt_Buf,2048);
	
	_bGetColor=true; //===>呼叫StringGridDrawCell()
	
    fclose(fps);

	
    //顯示記憶體規格
    Form1->LMemSpec->Caption="";
    Form1->Lab_XMP1->Caption="";
    Form1->Lab_XMP3->Caption="";
    Show_MemSpec();
    Show_XMPSpec();
    Show_EXPO();
	/*暫時跳過CRC編碼檢查
    //CRC編碼(檢查)
    bool ChkCRC16=true;
    int nCount = (buffer_temp1[0] & 0x80)? 117: 126;
    unsigned short chkcrc = 0;
    for (int i=0; i<nCount; i++)chkcrc = mkcrc16(chkcrc, buffer_temp1[i]);
    //MemoX->Lines->Add("CRC:"+IntToHex(chkcrc,4));
    int crcX=buffer_temp1[127];
    crcX=(crcX<<8) | buffer_temp1[126];
    //MemoX->Lines->Add(IntToHex(crcX,4));
    if(crcX!=chkcrc){
        ShowMessage("CRC檢查錯誤!\n請勿進行傳輸作業!");
        ChkCRC16=false;
    }
    nCount = 254;
    chkcrc = 0;
    for (int i=128; i<nCount; i++)chkcrc = mkcrc16(chkcrc, buffer_temp1[i]);
    //MemoX->Lines->Add("CRC:"+IntToHex(chkcrc,4));
    crcX=buffer_temp1[255];
    crcX=(crcX<<8) | buffer_temp1[254];
    //MemoX->Lines->Add(IntToHex(crcX,4));
    if(crcX!=chkcrc){
        ShowMessage("CRC檢查錯誤!\n請勿進行傳輸作業!");
        ChkCRC16=false;
    }
    StatusBar1->Panels->Items[0]->Text =SPD_name;
    */
}       
//---------------------------------------------------------------------------

void __fastcall TForm1::BitBtn3Click(TObject *Sender) //另存新檔
{
 AnsiString Save_name;
 long file_size;
    SaveDialog1->InitialDir= GetCurrentDir()+"\\";
    SaveDialog1->Title    =   "儲存SPD檔";
    SaveDialog1->Filter   =   "SPD   files   (*.spd)|*.spd" ;
    SaveDialog1->DefaultExt   =   ".spd";
    SaveDialog1->FilterIndex = 1; // start the dialog showing all files
    if(SaveDialog1->Execute()){
        Save_name=SaveDialog1->FileName;
        SaveDialog1->InitialDir="";
    }
    if(Save_name.IsEmpty())return;  /******按取消,不存檔*******/
    SaveDialog1->FileName="";
    if(FileExists(Save_name)){
        char idYesNo;
        idYesNo=MessageBox(NULL,"檔案已經存在，是否覆蓋?","Caution!",MB_YESNO+64);
        if(idYesNo==IDNO)return;
    }
    FILE *fps = fopen((Save_name).c_str(), "wt");
    if (fps == NULL) {
        MessageBox ( NULL,(Save_name.c_str()),"Error", MB_ICONSTOP );
        fclose(fps);
        return;
    }
    char Setbuf[60]; 
	int spaceCount = 0;
    memset(SPD_Buf,0,3264); //先將存檔後的3264位元的spd_buf設為0,所以不包含任何垃圾符號
    for(int i=0;i<64;i++)
	{ //執行64次,將一個格式化的字串寫入 Setbuf 中。這個字串包括了16個HEX,中間有空格,最後有換行符,迭代總共3264位元 
        sprintf(Setbuf," %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X \n",buffer_temp1[i*16+0],buffer_temp1[i*16+1],buffer_temp1[i*16+2],buffer_temp1[i*16+3],buffer_temp1[i*16+4],buffer_temp1[i*16+5],buffer_temp1[i*16+6],buffer_temp1[i*16+7],buffer_temp1[i*16+8],buffer_temp1[i*16+9],buffer_temp1[i*16+10],buffer_temp1[i*16+11],buffer_temp1[i*16+12],buffer_temp1[i*16+13],buffer_temp1[i*16+14],buffer_temp1[i*16+15]); 
		memcpy(&SPD_Buf[i*50],Setbuf,50); //使用 memcpy 函數將 Setbuf 中的50個字符復製到 SPD_Buf 中 (50個BYTES代表16組HEX+空白鍵+換行)
		memset(Setbuf,0,50);
    } //---->一行50BYTES*64列 +    每一行空白 = 3264BYTES
	
	fwrite(SPD_Buf,1,3200,fps); //這行使用 fwrite 函數將 SPD_Buf 中的3200個BYTES的數據寫入到檔案指標 fps 所指向的檔案中
    fclose(fps); //關閉檔案指標 fps，確保已完成對該檔案的寫入操作
    StatusBar1->Panels->Items[0]->Text="Save Done!";
}
//---------------------------------------------------------------------------
unsigned short TForm1::mkcrc16(unsigned short crc16, unsigned char ch)
{               crc16 = crc16 ^ ((unsigned short)ch << 8);
    for (int i=0; i<8; i++) {
        if (crc16 & 0x8000)
            crc16 = (crc16 << 1) ^ 0x1021;
        else
            crc16 <<= 1;
    }
    return crc16;
}
/*
unsigned short TForm1::DDR5mkcrc16(unsigned short crc16, unsigned char ch)
{          
		int Crc16 (char *ptr, int count)
		{
 		int crc, i;
		crc = 0;
		while (--count >= 0) {
 		crc = crc ^ (int)*ptr++ << 8;
		for (i = 0; i < 8; ++i)
 		if (crc & 0x8000)
 		crc = crc << 1 ^ 0x1021;
		else
 		crc = crc << 1;
 		}
		return (crc & 0xFFFF);
		}
		char spdBytes[] = { SPD_byte_0, SPD_byte_1, ..., SPD_byte_N-1 };
		int data16;
		data16 = Crc16 (spdBytes, sizeof(spdBytes));
		SPD_byte_510 = (char) (data16 & 0xFF);
		SPD_byte_511 = (char) (data16 >> 8);
}
*/
char TForm1::getnibbleL(char x){ //反推ascii回去hex碼(低位的字元)
    x=(x & 0xf); //x為一組字元,比如36,和0xf做 AND 運算後會清掉高位3留下低位6
    if(x<=9)return x+0x30; //如果剩下的x小於9,則+0x30可轉換為相應的字元,例如x=6,x+30=0x36(hex)---->Ascii為6
    else return x+0x37;//如果比9大就是那些a~f,加上37可對應相對的字元,例如c+37=43(hex)----->Ascii為C
}
char TForm1::getnibbleH(char x){//反推ascii回去hex碼(高位的字元)
    x=(x & 0xf0)>>4; //x為一組字元,比如36,和0xf0做 AND 運算後會清掉低位6留下高位3
    if(x<=9)return x+0x30; //如果3小於9,3加上30=33(hex)------>Ascii為3
    else return x+0x37; //如果a~f加上37為對應英文字母
}


void __fastcall TForm1::StringGrid1GetEditMask(TObject *Sender, int ACol,
      int ARow, AnsiString &Value)
{
	 Value = "CC";// 只能1個 A-Z, or a-z or 0-9      
}
//---------------------------------------------------------------------------
int first_key=0;
void __fastcall TForm1::StringGrid1KeyPress(TObject *Sender, char &Key)
{
	if((Key>='0' && Key<='9') || (Key>='A' && Key<='F')) Key=Key;
    else if((Key>='a' && Key<='f'))Key = std::toupper(Key);
	else return;
    if(first_key==0)first_key=1;
	//ShowMessage("first_key = 0");
    if(first_key==1){
        //StringGrid1->Cells[ChCol][ChRow]="";
        StringGrid1->Cells[ChCol][ChRow] = Key;
        first_key=2;
		//ShowMessage("first_key = 1");
    }else{
		//ShowMessage("first_key = 2");
        StringGrid1->Cells[ChCol][ChRow]=StringGrid1->Cells[ChCol][ChRow]+Key;
        //將輸入文字轉成Hex數字
        buffer_temp1[((ChRow*16)+ChCol)+512]=mstrToByte(StringGrid1->Cells[ChCol][ChRow].c_str(),0);
        //將輸入更新至Txt_Buf
        Txt_Buf[((ChRow*16*2)+ChCol*2)+512]= getnibbleH((char)(buffer_temp1[((ChRow*16)+ChCol)+512] & 0xff));
        Txt_Buf[((ChRow*16*2)+ChCol*2+1)+512]= getnibbleL((char)(buffer_temp1[((ChRow*16)+ChCol)+512] & 0xff));
        //顯示記憶體規格
        Form1->LMemSpec->Caption="";
        Form1->Lab_XMP1->Caption="";
        Form1->Lab_XMP3->Caption="";
        Show_MemSpec();
        Show_XMPSpec();
		Show_EXPO();
		//CRC編碼(檢查)
        bool ChkCRC16=true;
		int nCount = 702;
        unsigned short chkcrc = 0;
        for (int i=640; i<nCount; i++)chkcrc = mkcrc16(chkcrc, buffer_temp1[i]);
		//ShowMessage(IntToHex(chkcrc,2));
        //MemoX->Lines->Add("CRC:"+IntToHex(chkcrc,4));
        //更新到儲存格
        StringGrid1->Cells[14][11]=IntToHex((chkcrc&0xff),2);//702
        StringGrid1->Cells[15][11]=IntToHex((chkcrc&0xff00)>>8,2);//703
        
        //更新到buffer_temp1[702][703]
        buffer_temp1[702]= chkcrc&0xff;
        buffer_temp1[703]= (chkcrc&0xff00)>>8;
		Txt_Buf[702*2]= getnibbleH((char)(buffer_temp1[702] & 0xff));
        Txt_Buf[702*2+1]= getnibbleL((char)(buffer_temp1[702] & 0xff));

		Txt_Buf[703*2]= getnibbleH((char)(buffer_temp1[703] & 0xff));
        Txt_Buf[703*2+1]= getnibbleL((char)(buffer_temp1[703] & 0xff));
		//-------------------------------------------profile1(704~767)
        nCount = 766;
        chkcrc = 0;
        for (int i=704; i<nCount; i++)chkcrc = mkcrc16(chkcrc, buffer_temp1[i]);
		//ShowMessage(IntToHex(chkcrc,2));
        //MemoX->Lines->Add("CRC:"+IntToHex(chkcrc,4));
        //更新到儲存格
        StringGrid1->Cells[14][15]=IntToHex((chkcrc&0xff),2);//766
        StringGrid1->Cells[15][15]=IntToHex((chkcrc&0xff00)>>8,2);//767

		//更新到buffer_temp1[766][767]
        buffer_temp1[766]= chkcrc&0xff;
        buffer_temp1[767]= (chkcrc&0xff00)>>8;

		Txt_Buf[766*2]= getnibbleH((char)(buffer_temp1[766] & 0xff));
        Txt_Buf[766*2+1]= getnibbleL((char)(buffer_temp1[766] & 0xff));

		Txt_Buf[767*2]= getnibbleH((char)(buffer_temp1[767] & 0xff));
        Txt_Buf[767*2+1]= getnibbleL((char)(buffer_temp1[767] & 0xff));
        //------------------------------------------------profile2(768~831)
        nCount = 830;
        chkcrc = 0;
        for (int i=768; i<nCount; i++)chkcrc = mkcrc16(chkcrc, buffer_temp1[i]);
        //MemoX->Lines->Add("CRC:"+IntToHex(chkcrc,4));
        //更新到儲存格
        StringGrid1->Cells[14][19]=IntToHex((chkcrc&0xff),2);//830
        StringGrid1->Cells[15][19]=IntToHex((chkcrc&0xff00)>>8,2);//831

		//更新到buffer_temp1[766][767]
        buffer_temp1[830]= chkcrc&0xff;
        buffer_temp1[831]= (chkcrc&0xff00)>>8;

		Txt_Buf[830*2]= getnibbleH((char)(buffer_temp1[830] & 0xff));
        Txt_Buf[830*2+1]= getnibbleL((char)(buffer_temp1[830] & 0xff));

		Txt_Buf[831*2]= getnibbleH((char)(buffer_temp1[831] & 0xff));
        Txt_Buf[831*2+1]= getnibbleL((char)(buffer_temp1[831] & 0xff));
		//------------------------------------------------EXPO1~EXPO2(832~957)
		if(EXPOBOX->Checked==true || EXPOBOX2->Checked==true){
		nCount = 958;
        chkcrc = 0;
        for (int i=832; i<nCount; i++)chkcrc = mkcrc16(chkcrc, buffer_temp1[i]);
        //更新到儲存格
        StringGrid1->Cells[14][27]=IntToHex((chkcrc&0xff),2);//958
        StringGrid1->Cells[15][27]=IntToHex((chkcrc&0xff00)>>8,2);//959

		//更新到buffer_temp1[894][895]
        buffer_temp1[958]= chkcrc&0xff;
        buffer_temp1[959]= (chkcrc&0xff00)>>8;

		Txt_Buf[958*2]= getnibbleH((char)(buffer_temp1[958] & 0xff));
        Txt_Buf[958*2+1]= getnibbleL((char)(buffer_temp1[958] & 0xff));

		Txt_Buf[959*2]= getnibbleH((char)(buffer_temp1[959] & 0xff));
        Txt_Buf[959*2+1]= getnibbleL((char)(buffer_temp1[959] & 0xff));
		}
		else
		{
		//------------------------------------------------profile3(832~895)
        	nCount = 894;
        	chkcrc = 0;
        	for (int i=832; i<nCount; i++)chkcrc = mkcrc16(chkcrc, buffer_temp1[i]);
			//ShowMessage(IntToHex(chkcrc,2));
        	//MemoX->Lines->Add("CRC:"+IntToHex(chkcrc,4));
        	//更新到儲存格
        	StringGrid1->Cells[14][23]=IntToHex((chkcrc&0xff),2);//894
        	StringGrid1->Cells[15][23]=IntToHex((chkcrc&0xff00)>>8,2);//895

			//更新到buffer_temp1[894][895]
        	buffer_temp1[894]= chkcrc&0xff;
        	buffer_temp1[895]= (chkcrc&0xff00)>>8;

			Txt_Buf[894*2]= getnibbleH((char)(buffer_temp1[894] & 0xff));
        	Txt_Buf[894*2+1]= getnibbleL((char)(buffer_temp1[894] & 0xff));

			Txt_Buf[895*2]= getnibbleH((char)(buffer_temp1[895] & 0xff));
        	Txt_Buf[895*2+1]= getnibbleL((char)(buffer_temp1[895] & 0xff));
		//------------------------------------------------profile4(896~959)
        	nCount = 958;
        	chkcrc = 0;
        	for (int i=896; i<nCount; i++)chkcrc = mkcrc16(chkcrc, buffer_temp1[i]);
        	//MemoX->Lines->Add("CRC:"+IntToHex(chkcrc,4));
        	//更新到儲存格
        	StringGrid1->Cells[14][27]=IntToHex((chkcrc&0xff),2);//958
        	StringGrid1->Cells[15][27]=IntToHex((chkcrc&0xff00)>>8,2);//959

			//更新到buffer_temp1[958][959]
        	buffer_temp1[958]= chkcrc&0xff;
        	buffer_temp1[959]= (chkcrc&0xff00)>>8;

			Txt_Buf[958*2]= getnibbleH((char)(buffer_temp1[958] & 0xff));
        	Txt_Buf[958*2+1]= getnibbleL((char)(buffer_temp1[958] & 0xff));

			Txt_Buf[959*2]= getnibbleH((char)(buffer_temp1[959] & 0xff));
        	Txt_Buf[959*2+1]= getnibbleL((char)(buffer_temp1[959] & 0xff));
		}
		//------------------------------------------------profile5(960~1023)
        nCount = 1022;
        chkcrc = 0;
        for (int i=960; i<nCount; i++)chkcrc = mkcrc16(chkcrc, buffer_temp1[i]);
        //MemoX->Lines->Add("CRC:"+IntToHex(chkcrc,4));
        //更新到儲存格
        StringGrid1->Cells[14][31]=IntToHex((chkcrc&0xff),2);//1022
        StringGrid1->Cells[15][31]=IntToHex((chkcrc&0xff00)>>8,2);//1023

		//更新到buffer_temp1[1022][1023]
        buffer_temp1[1022]= chkcrc&0xff;
        buffer_temp1[1023]= (chkcrc&0xff00)>>8;

		Txt_Buf[1022*2]= getnibbleH((char)(buffer_temp1[1022] & 0xff));
        Txt_Buf[1022*2+1]= getnibbleL((char)(buffer_temp1[1022] & 0xff));

		Txt_Buf[1023*2]= getnibbleH((char)(buffer_temp1[1023] & 0xff));
        Txt_Buf[1023*2+1]= getnibbleL((char)(buffer_temp1[1023] & 0xff));
		//ShowMessage("更改crc");
		first_key=0;
    }
}
//---------------------------------------------------------------------------

void __fastcall TForm1::StringGrid1SelectCell(TObject *Sender, int ACol,
      int ARow, bool &CanSelect)
{
	ChRow=ARow; //被選中的列
    ChCol=ACol; //被選中的行         
}
//---------------------------------------------------------------------------


void __fastcall TForm1::StringGrid2GetEditMask(TObject *Sender, int ACol,
      int ARow, AnsiString &Value)
{
	 Value = "CC";// 只能1個 A-Z, or a-z or 0-9        
}
//---------------------------------------------------------------------------

void __fastcall TForm1::StringGrid2KeyPress(TObject *Sender, char &Key)
{
	if((Key>='0' && Key<='9') || (Key>='A' && Key<='F')) Key=Key;
    else if((Key>='a' && Key<='f'))Key = std::toupper(Key);
	else return;
    if(first_key==0)first_key=1;
	//ShowMessage("first_key = 0");
    if(first_key==1){
        //StringGrid1->Cells[ChCol][ChRow]="";
        StringGrid2->Cells[ChCol][ChRow] = Key;
		//ShowMessage("first_key = 1");
        first_key=2;
    }else{
		//ShowMessage("first_key = 2");
        StringGrid2->Cells[ChCol][ChRow]=StringGrid2->Cells[ChCol][ChRow]+Key;
        //將輸入文字轉成Hex數字
        buffer_temp1[(ChRow*16)+ChCol]=mstrToByte(StringGrid2->Cells[ChCol][ChRow].c_str(),0);
        //將輸入更新至Txt_Buf
        Txt_Buf[(ChRow*16*2)+ChCol*2]= getnibbleH((char)(buffer_temp1[(ChRow*16)+ChCol] & 0xff));
        Txt_Buf[(ChRow*16*2)+ChCol*2+1]= getnibbleL((char)(buffer_temp1[(ChRow*16)+ChCol] & 0xff));
        //顯示記憶體規格
        Form1->LMemSpec->Caption="";
        Form1->Lab_XMP1->Caption="";
        Form1->Lab_XMP3->Caption="";
        Show_MemSpec();
        Show_XMPSpec();
		Show_EXPO();
		//CRC編碼(檢查)--->BYTE 510 511
        bool ChkCRC16=true;
		int nCount = 510;
        unsigned short chkcrc = 0;
        for (int i=0; i<nCount; i++)chkcrc = mkcrc16(chkcrc, buffer_temp1[i]);
		//ShowMessage(IntToHex(chkcrc,2));
        //MemoX->Lines->Add("CRC:"+IntToHex(chkcrc,4));
        //更新到儲存格
        StringGrid2->Cells[14][31]=IntToHex((chkcrc&0xff),2);//510
        StringGrid2->Cells[15][31]=IntToHex((chkcrc&0xff00)>>8,2);//511
        
        //更新到buffer_temp1[510][511]
        buffer_temp1[510]= chkcrc&0xff;
        buffer_temp1[511]= (chkcrc&0xff00)>>8;
		Txt_Buf[510*2]= getnibbleH((char)(buffer_temp1[510] & 0xff));
        Txt_Buf[510*2+1]= getnibbleL((char)(buffer_temp1[510] & 0xff));

		Txt_Buf[511*2]= getnibbleH((char)(buffer_temp1[511] & 0xff));
        Txt_Buf[511*2+1]= getnibbleL((char)(buffer_temp1[511] & 0xff));
        //ShowMessage("更改crc");
        first_key=0;
		
    }              
}
//---------------------------------------------------------------------------

void __fastcall TForm1::StringGrid2SelectCell(TObject *Sender, int ACol,
      int ARow, bool &CanSelect)
{
	ChRow=ARow; //被選中的列
    ChCol=ACol; //被選中的行       
}
//----------------------------------------------------------------------------

void __fastcall TForm1::EXPOBOXClick(TObject *Sender)
{
	if(EXPOBOX->Checked==true || EXPOBOX2->Checked==true)
		RadioButton3->Checked=false;
}
//---------------------------------------------------------------------------

void __fastcall TForm1::RadioButton3Click(TObject *Sender)
{
        RadioButton3->Checked = true; 
		EXPOBOX->Checked=false;
        EXPOBOX2->Checked=false;  
        CheckBox4->Checked = false;     
}
//---------------------------------------------------------------------------

void TForm1::Show_MemSpec(void){
    AnsiString strType,strType2,strType3,strType4,strType5,strType6;
    int     nType;
    int     nRanks;
    int     nDieWidth;
    int     nBusWidth;
    int     nExtWidth;
    float   nTiming;
    int   nCycleTime;
    float   tCAS;
    float   tRP;
    float   tRCD;
    float   tRAS;
    float   tRFC;
    float   tRC;
    float   tRTP;
    float   tWR;

    float   tRFC1;
    float   tRFC2;
    float   tRFCsb;

    float   tRRD_L;
    float   tCCD_L;
    float   tFAW;
    int     nVal;
    int     package;
    char    bFTB;
    int     nSizeMB;
    int     TotalSizeMB;
    AnsiString sTemp;
    // is DDR5
        switch (buffer_temp1[234] ) { //rank數量
        default:
        case 0x00:     nRanks = 1;  break;
        case 0x08:     nRanks = 2; break;
        case 0x10:     nRanks = 3; break;
        case 0x18:     nRanks = 4; break;
        }
        
        switch (buffer_temp1[6] >>5) { //*多少的IC
        default:
        case 0:     nDieWidth = 4;     break;
        case 1:     nDieWidth = 8;     break;
        case 2:     nDieWidth = 16;    break;
        case 3:     nDieWidth = 32;    break;
        }

        nVal = (int)(2000000/(buffer_temp1[21]<<8 | buffer_temp1[20])); //頻率
        nCycleTime =  (int)(buffer_temp1[21]<<8 | buffer_temp1[20]); //多少ps
        //ShowMessage("val="+IntToStr(nCycleTime));

        tCAS = (int)(16000 / nCycleTime); //tAA
        if((int)tCAS %2 !=0) tCAS+=1; //第一碼不為奇數
        tRCD = (int)(16000 / nCycleTime); 

        tRP = (int)(16000 / nCycleTime); 

        tRAS =  (int)(32000 / nCycleTime); 


        //MemoX->Lines->Add(FloatToStrF(nCycleTime,ffFixed,5,5)+","+FloatToStrF(tCAS,ffFixed,5,5)+","+FloatToStrF(tRP,ffFixed,5,5)+","+FloatToStrF(tRCD,ffFixed,5,5)+","+FloatToStrF(tRAS,ffFixed,5,5)+","+FloatToStrF(tRFC,ffFixed,5,5));;

        strType = "DDR5, ";
        strType=strType+"1.1V, ";  //增加standard 顯示電壓規格
        switch (buffer_temp1[3] & 0xf) { //dimm的類型
        case 1:     strType += "RDIMM, ";    break ;
        case 2:     strType += "UDIMM, ";    break;
        case 3:     strType += "SO-DIMM, ";   break;
        case 4:     strType += "LRDIMM, ";    break;
        case 7:     strType += "Mini-RDIMM, ";    break;
        }
        switch (buffer_temp1[4] & 0xf) {//單位(GB)
        case 0:     nSizeMB = 0;    break;
        case 1:     nSizeMB = 4;    break;
        case 2:     nSizeMB = 8;    break;
        case 3:     nSizeMB = 12;    break;
        case 4:     nSizeMB = 16;    break;
        case 5:     nSizeMB = 24;    break;
        case 6:     nSizeMB = 32;    break;
        case 7:     nSizeMB = 48;    break;
        case 8:     nSizeMB = 64;    break;
        }
        TotalSizeMB = nSizeMB*nRanks;
        if      (!TotalSizeMB)       strType = strType + "Unknow size";
        else   strType =strType + IntToStr((TotalSizeMB)) +" GB";

        strType2=IntToStr(nVal)+" MHz / CL"+IntToStr((int)(tCAS))+"-"+IntToStr((int)(tRCD))+"-"+IntToStr((int)(tRP))+"-"+IntToStr((int)(tRAS));
        strType3="Ranks:"+IntToStr(nRanks)+",";
        switch (buffer_temp1[5] & 0xf) {
        case 0:     strType3 = strType3+" Col:16";   break;
        case 1:     strType3 = strType3+" Col:17";   break;
        case 2:     strType3 = strType3+" Col:18";  break;
        }
        switch (buffer_temp1[5]>>5) {
        case 0:     strType3 = strType3+" Row:10";   break;
        case 1:     strType3 = strType3+" Row:11";   break;
        }
        
        int ic_size=nSizeMB/nDieWidth;
        //Form1->MemoX->Lines->Add("nRanks:"+IntToStr(nRanks)+"nSizeMB:"+IntToStr(nSizeMB)+"package:"+IntToStr(package)+" nDieWidth:"+IntToStr(nDieWidth));
        
        strType3 = strType3+" "+IntToStr(ic_size)+"Gx"+IntToStr(nDieWidth);
        //------------------------------------------tRC
        tRC = (int)(buffer_temp1[39]<<8 | buffer_temp1[38]) / nCycleTime;
        //------------------------------------------tWR
        tWR = (int)(buffer_temp1[41]<<8 | buffer_temp1[40]) / nCycleTime;
        strType4 = "tRC:"+IntToStr((int)(tRC))+" "+"tWR:"+IntToStr((int)(tWR));
        //----------------------------------------trfc1 trfc2 trfcsb(ns)
        tRFC1 = ((int)(buffer_temp1[43]<<8 | buffer_temp1[42]))*1000 / nCycleTime;

        tRFC2 = ((int)(buffer_temp1[45]<<8 | buffer_temp1[44]))*1000 / nCycleTime;

        tRFCsb = ((int)(buffer_temp1[47]<<8 | buffer_temp1[46]))*1000 / nCycleTime;
        strType5="tRFC1:"+IntToStr((int)(tRFC1))+"    "+"tRFC2:"+IntToStr((int)(tRFC2))+"    "+"tRFCsb:"+IntToStr((int)(tRFCsb));
        //------------------------------------------tRRD_L tCCD_L tFAW
        tRRD_L = (int)(buffer_temp1[71]<<8 | buffer_temp1[70]) / nCycleTime;

        tCCD_L = (int)(buffer_temp1[74]<<8 | buffer_temp1[73]) / nCycleTime;

        tFAW =  (int)(buffer_temp1[83]<<8 | buffer_temp1[82]) / nCycleTime;
        
        strType6="tRRD_L:"+IntToStr((int)(tRRD_L))+"    "+"tCCD_L:"+IntToStr((int)(tCCD_L))+"    "+"tFAW:"+IntToStr((int)(tFAW));

    LMemSpec->Caption=strType+"\n"+strType2+"\n"+strType3+"\n"+strType4+"\n"+strType5+"\n"+strType6;
}


void TForm1::Show_XMPSpec(void){
    //XMP1
    AnsiString strType,strType2,strType3,strType4;
    int     nType;
    int     nRanks;
    int     nDieWidth;
    int     nBusWidth;
    int     nExtWidth;
    float   nTiming;
    float   nCycleTime;
    float   tCAS;
    float   tRP;
    float   tRCD;
    float   tRAS;
    float   tRC;
    float   tRFC;
    float   tWR;
    float   tRFC1;
    float   tRFC2;
    float   tRFCsb;
    int     nVal;
    char    bFTB;
    int     nSizeMB;
    AnsiString sTemp;
    int     itmp;
    float   ftmp;
    AnsiString sVoltage;
    //-------------------------------------------------------------------------------------------------
    //XMP2
    AnsiString DstrType,DstrType2,DstrType3,DstrType4;
    int     DnType;
    int     DnRanks;
    int     DnDieWidth;
    int     DnBusWidth;
    int     DnExtWidth;
    float   DnTiming;
    float   DnCycleTime;
    float   DtCAS;
    float   DtRP;
    float   DtRCD;
    float   DtRAS;
    float   DtRC;
    float   DtRFC;
    float   DtWR;
    float   DtRFC1;
    float   DtRFC2;
    float   DtRFCsb;
    int     DnVal;
    char    DbFTB;
    int     DnSizeMB;
    AnsiString DsTemp;
    int     Ditmp;
    float   Dftmp;
    //*---------------------------------------------------------------------------------------------------

    if(buffer_temp1[640]==0x0c && buffer_temp1[641]==0x4A){
        strType="XMP:";
    }else{
        Lab_XMP1->Caption="";
        Lab_XMP3->Caption="";
        return;
    }

    //計算timing 一切根本就是要先求出nCycleTime
    nVal = (int)(2000000/(buffer_temp1[710]<<8 | buffer_temp1[709])); //頻率
    nCycleTime =  (int)(buffer_temp1[710]<<8 | buffer_temp1[709]); //多少ps

    //電壓
    ftmp=(buffer_temp1[705]&0xf);
    ftmp = (int)(ftmp)*5;
    if(ftmp < 10) Lab_XMP1->Caption = "1.05V ";
    else Lab_XMP1->Caption = "1."+IntToStr((int)(ftmp))+"V ";

    Lab_XMP1->Caption=Lab_XMP1->Caption+IntToStr(nVal)+" MHz / CL";

    //tCAS = nTiming;
    tCAS = (int)(buffer_temp1[718]<<8 | buffer_temp1[717]) / nCycleTime;

    tRCD = (int)(buffer_temp1[720]<<8 | buffer_temp1[719]) / nCycleTime;

    tRP  = (int)(buffer_temp1[722]<<8 | buffer_temp1[721]) / nCycleTime;

    tRAS = (int)(buffer_temp1[724]<<8 | buffer_temp1[723]) / nCycleTime;

    strType=IntToStr((int)(tCAS))+"-"+IntToStr((int)(tRCD))+"-"+IntToStr((int)(tRP))+"-"+IntToStr((int)(tRAS));
    Lab_XMP1->Caption=Lab_XMP1->Caption+strType+"\n";
    //-----------------------------------------tRC
    tRC  =  (int)(buffer_temp1[726]<<8 | buffer_temp1[725]) / nCycleTime;
    strType2="tRC:"+IntToStr((int)(tRC))+" ";
    Lab_XMP1->Caption=Lab_XMP1->Caption+strType2+"\n";
    //-----------------------------------------tWR
    tWR  =  (int)(buffer_temp1[728]<<8 | buffer_temp1[727]) / nCycleTime;
    strType3="tWR:"+IntToStr((int)(tWR))+" ";
    Lab_XMP1->Caption=Lab_XMP1->Caption+strType3+"\n";
    //----------------------------------------trfc1 trfc2 trfcsb
    tRFC1 = ((int)(buffer_temp1[730]<<8 | buffer_temp1[729]))*1000 / nCycleTime;

    tRFC2 = ((int)(buffer_temp1[732]<<8 | buffer_temp1[731]))*1000 / nCycleTime;

    tRFCsb = ((int)(buffer_temp1[734]<<8 | buffer_temp1[733]))*1000 / nCycleTime;
    strType4="tRFC1:"+IntToStr((int)(tRFC1))+" "+"tRFC2:"+IntToStr((int)(tRFC2))+" "+"tRFCsb:"+IntToStr((int)(tRFCsb));
    Lab_XMP1->Caption=Lab_XMP1->Caption+strType4;
    //ShowMessage("XMP1!");
    //***********************************************************************
    //XMP2

    if(buffer_temp1[643] == 0x03)DstrType="XMP2:";
    else return;
    //計算timing 一切根本就是要先求出nCycleTime
    DnVal = (int)(2000000/(buffer_temp1[774]<<8 | buffer_temp1[773])); //頻率
    DnCycleTime =  (int)(buffer_temp1[774]<<8 | buffer_temp1[773]); //多少ps

    //電壓
    Dftmp=(buffer_temp1[769]&0xf);
    Dftmp = (int)(Dftmp)*5;
    if(Dftmp < 10) Lab_XMP3->Caption = "1.05V ";
    else Lab_XMP3->Caption = "1."+IntToStr((int)(Dftmp))+"V ";

    Lab_XMP3->Caption=Lab_XMP3->Caption+IntToStr(DnVal)+" MHz / CL";

    //tCAS = nTiming;
    DtCAS = (int)(buffer_temp1[782]<<8 | buffer_temp1[781]) / DnCycleTime;

    DtRCD = (int)(buffer_temp1[784]<<8 | buffer_temp1[783]) / DnCycleTime;

    DtRP  = (int)(buffer_temp1[786]<<8 | buffer_temp1[785]) / DnCycleTime;

    DtRAS = (int)(buffer_temp1[788]<<8 | buffer_temp1[787]) / DnCycleTime;

    DstrType=IntToStr((int)(DtCAS))+"-"+IntToStr((int)(DtRCD))+"-"+IntToStr((int)(DtRP))+"-"+IntToStr((int)(DtRAS));
    Lab_XMP3->Caption=Lab_XMP3->Caption+DstrType+"\n";
    //-----------------------------------------tRC
    DtRC  =  (int)(buffer_temp1[790]<<8 | buffer_temp1[789]) / DnCycleTime;
    DstrType2="tRC:"+IntToStr((int)(DtRC))+" ";
    Lab_XMP3->Caption=Lab_XMP3->Caption+DstrType2+"\n";
    //-----------------------------------------tWR
    DtWR  =  (int)(buffer_temp1[792]<<8 | buffer_temp1[791]) / DnCycleTime;
    DstrType3="tWR:"+IntToStr((int)(DtWR))+" ";
    Lab_XMP3->Caption=Lab_XMP3->Caption+DstrType3+"\n";
    //----------------------------------------trfc1 trfc2 trfcsb
    DtRFC1= ((int)(buffer_temp1[794]<<8 | buffer_temp1[793]))*1000 / DnCycleTime;

    DtRFC2 = ((int)(buffer_temp1[796]<<8 | buffer_temp1[795]))*1000 / DnCycleTime;

    DtRFCsb = ((int)(buffer_temp1[798]<<8 | buffer_temp1[797]))*1000 / DnCycleTime;
    DstrType4="tRFC1:"+IntToStr((int)(DtRFC1))+" "+"tRFC2:"+IntToStr((int)(DtRFC2))+" "+"tRFCsb:"+IntToStr((int)(DtRFCsb));
    Lab_XMP3->Caption=Lab_XMP3->Caption+DstrType4;
    //ShowMessage("XMP2!");
}
void TForm1::Show_EXPO(void){
    AnsiString EstrType,EstrType2,EstrType3,EstrType4;
    int     EnType;
    int     EnRanks;
    int     EnDieWidth;
    int     EnBusWidth;
    int     EnExtWidth;
    float   EnTiming;
    float   EnCycleTime;
    float   EtCAS;
    float   EtRP;
    float   EtRCD;
    float   EtRAS;
    float   EtRC;
    float   EtRFC;
    float   EtWR;
    float   EtRFC1;
    float   EtRFC2;
    float   EtRFCsb;
    int     EnVal;
    char    EbFTB;
    int     EnSizeMB;
    AnsiString EsTemp;
    int     Eitmp;
    float   Eftmp;
    AnsiString EsVoltage;

    if(buffer_temp1[832]==0x45 && buffer_temp1[833]==0x58){
        EstrType="Expo:";
    }else{
        Lab_Expo->Caption="";
        return;
    }

    //計算timing 一切根本就是要先求出nCycleTime
    EnVal = (int)(2000000/(buffer_temp1[847]<<8 | buffer_temp1[846])); //頻率
    EnCycleTime =  (int)(buffer_temp1[847]<<8 | buffer_temp1[846]); //多少ps

    //電壓
    Eftmp=(buffer_temp1[842]&0xf);
    Eftmp = (int)(Eftmp)*5;
    if(Eftmp < 10) Lab_Expo->Caption = "1.05V ";
    else Lab_Expo->Caption = "1."+IntToStr((int)(Eftmp))+"V ";

    Lab_Expo->Caption=Lab_Expo->Caption+IntToStr(EnVal)+" MHz / CL";

    //tCAS = nTiming;
    EtCAS = (int)(buffer_temp1[849]<<8 | buffer_temp1[848]) / EnCycleTime;

    EtRCD = (int)(buffer_temp1[851]<<8 | buffer_temp1[850]) / EnCycleTime;

    EtRP  = (int)(buffer_temp1[853]<<8 | buffer_temp1[852]) / EnCycleTime;

    EtRAS = (int)(buffer_temp1[855]<<8 | buffer_temp1[854]) / EnCycleTime;

    EstrType=IntToStr((int)(EtCAS))+"-"+IntToStr((int)(EtRCD))+"-"+IntToStr((int)(EtRP))+"-"+IntToStr((int)(EtRAS));
    Lab_Expo->Caption=Lab_Expo->Caption+EstrType+"\n";
    //-----------------------------------------tRC
    EtRC  =  (int)(buffer_temp1[857]<<8 | buffer_temp1[856]) / EnCycleTime;
    EstrType2="tRC:"+IntToStr((int)(EtRC))+" ";
    Lab_Expo->Caption=Lab_Expo->Caption+EstrType2+"\n";
    //-----------------------------------------tWR
    EtWR  =  (int)(buffer_temp1[859]<<8 | buffer_temp1[858]) / EnCycleTime;
    EstrType3="tWR:"+IntToStr((int)(EtWR))+" ";
    Lab_Expo->Caption=Lab_Expo->Caption+EstrType3+"\n";
    //----------------------------------------trfc1 trfc2 trfcsb
    EtRFC1 = ((int)(buffer_temp1[861]<<8 | buffer_temp1[860]))*1000 / EnCycleTime;

    EtRFC2 = ((int)(buffer_temp1[863]<<8 | buffer_temp1[862]))*1000 / EnCycleTime;

    EtRFCsb = ((int)(buffer_temp1[865]<<8 | buffer_temp1[864]))*1000 / EnCycleTime;
    EstrType4="tRFC1:"+IntToStr((int)(EtRFC1))+" "+"tRFC2:"+IntToStr((int)(EtRFC2))+" "+"tRFCsb:"+IntToStr((int)(EtRFCsb));
    Lab_Expo->Caption=Lab_Expo->Caption+EstrType4;
    
    //---------------------------------------------------expo2
     AnsiString E2strType,E2strType2,E2strType3,E2strType4;
    int     E2nType;
    int     E2nRanks;
    int     E2nDieWidth;
    int     E2nBusWidth;
    int     E2nExtWidth;
    float   E2nTiming;
    float   E2nCycleTime;
    float   E2tCAS;
    float   E2tRP;
    float   E2tRCD;
    float   E2tRAS;
    float   E2tRC;
    float   E2tRFC;
    float   E2tWR;
    float   E2tRFC1;
    float   E2tRFC2;
    float   E2tRFCsb;
    int     E2nVal;
    char    E2bFTB;
    int     E2nSizeMB;
    AnsiString E2sTemp;
    int     E2itmp;
    float   E2ftmp;
    AnsiString E2sVoltage;

     if(buffer_temp1[837]==0x33){ //開啟expo2
        E2strType="Expo2:";
    }else{
        Lab_Expo2->Caption="";
        return;
    }

    //計算timing 一切根本就是要先求出nCycleTime
    E2nVal = (int)(2000000/(buffer_temp1[887]<<8 | buffer_temp1[886])); //頻率
    E2nCycleTime =  (int)(buffer_temp1[887]<<8 | buffer_temp1[886]); //多少ps

    //電壓
    E2ftmp=(buffer_temp1[882]&0xf);
    E2ftmp = (int)(E2ftmp)*5;
    if(E2ftmp < 10) Lab_Expo2->Caption = "1.05V ";
    else Lab_Expo2->Caption = "1."+IntToStr((int)(E2ftmp))+"V ";

    Lab_Expo2->Caption=Lab_Expo2->Caption+IntToStr(E2nVal)+" MHz / CL";

    //tCAS = nTiming;
    E2tCAS = (int)(buffer_temp1[889]<<8 | buffer_temp1[888]) / E2nCycleTime;

    E2tRCD = (int)(buffer_temp1[891]<<8 | buffer_temp1[890]) / E2nCycleTime;

    E2tRP  = (int)(buffer_temp1[893]<<8 | buffer_temp1[892]) / E2nCycleTime;

    E2tRAS = (int)(buffer_temp1[895]<<8 | buffer_temp1[894]) / E2nCycleTime;

    E2strType=IntToStr((int)(E2tCAS))+"-"+IntToStr((int)(E2tRCD))+"-"+IntToStr((int)(E2tRP))+"-"+IntToStr((int)(E2tRAS));
    Lab_Expo2->Caption=Lab_Expo2->Caption+E2strType+"\n";
    //-----------------------------------------tRC
    E2tRC  =  (int)(buffer_temp1[897]<<8 | buffer_temp1[896]) / E2nCycleTime;
    E2strType2="tRC:"+IntToStr((int)(E2tRC))+" ";
    Lab_Expo2->Caption=Lab_Expo2->Caption+E2strType2+"\n";
    //-----------------------------------------tWR
    E2tWR  =  (int)(buffer_temp1[899]<<8 | buffer_temp1[898]) / E2nCycleTime;
    E2strType3="tWR:"+IntToStr((int)(E2tWR))+" ";
    Lab_Expo2->Caption=Lab_Expo2->Caption+E2strType3+"\n";
    //----------------------------------------trfc1 trfc2 trfcsb
    E2tRFC1 = ((int)(buffer_temp1[901]<<8 | buffer_temp1[900]))*1000 / E2nCycleTime;

    E2tRFC2 = ((int)(buffer_temp1[903]<<8 | buffer_temp1[902]))*1000 / E2nCycleTime;

    E2tRFCsb = ((int)(buffer_temp1[905]<<8 | buffer_temp1[904]))*1000 / E2nCycleTime;
    E2strType4="tRFC1:"+IntToStr((int)(E2tRFC1))+" "+"tRFC2:"+IntToStr((int)(E2tRFC2))+" "+"tRFCsb:"+IntToStr((int)(E2tRFCsb));
    Lab_Expo2->Caption=Lab_Expo2->Caption+E2strType4;
}
void __fastcall TForm1::StringGrid2DrawCell(TObject *Sender, int ACol,
      int ARow, TRect &Rect, TGridDrawState State)     
{
	//---------------基頻--------------------
	 if(!_bGetColor)return;
    
   if((ARow == 12)&& (ACol == 2||ACol == 3)){//SPD HUB ID
        StringGrid2->Canvas->Brush->Color=clYellow; 
        StringGrid2->Canvas->Font->Color=clBlack;
        StringGrid2->Canvas->FillRect(Rect);
        StringGrid2->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid2->Cells[ACol][ARow]);
    }
   if((ARow == 12)&& (ACol == 6||ACol == 7)){//PMIC ID
        StringGrid2->Canvas->Brush->Color=clYellow; 
        StringGrid2->Canvas->Font->Color=clBlack;
        StringGrid2->Canvas->FillRect(Rect);
        StringGrid2->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid2->Cells[ACol][ARow]);
    }
   if((ARow == 2)&& (ACol == 10||ACol == 11||ACol == 12||ACol == 13||ACol == 14||ACol == 15)){//trfc1 trfc2 trfcsb
        StringGrid2->Canvas->Brush->Color=clFuchsia; 
        StringGrid2->Canvas->Font->Color=clBlack;
        StringGrid2->Canvas->FillRect(Rect);
        StringGrid2->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid2->Cells[ACol][ARow]);
    } 
    if((ARow == 31)&& (ACol == 14||ACol == 15)){//CRC
        StringGrid2->Canvas->Brush->Color=clLime;
        StringGrid2->Canvas->Font->Color=clBlack;
        StringGrid2->Canvas->FillRect(Rect);
        StringGrid2->Canvas->TextOutA(Rect.Left+2,Rect.Top+2,StringGrid2->Cells[ACol][ARow]);
    }   
    
}
//---------------------------------------------------------------------------


void __fastcall TForm1::RadioButton4Click(TObject *Sender)
{
	if (RadioButton4->Checked == true)
    {
        StringGrid2->Cells[2][12] = "86";
        StringGrid2->Cells[3][12] = "32";
        
    // 直接更新 Txt_Buf 
    Txt_Buf[(12 * 16 * 2) + 2 * 2] = '8'; // 第一個字符
    Txt_Buf[(12 * 16 * 2) + 2 * 2 + 1] = '6'; // 第二個字符
    Txt_Buf[(12 * 16 * 2) + 3 * 2] = '3'; // 第一個字符
    Txt_Buf[(12 * 16 * 2) + 3 * 2 + 1] = '2'; // 第二個字符
    
	


    //直接更新 buffer_temp1
    buffer_temp1[(12 * 16) + 2] = 0x86; // HEX 碼 0x58
    buffer_temp1[(12 * 16) + 3] = 0x32; // HEX 碼 0x30
    

    //CRC編碼(檢查)--->BYTE 510 511
       	bool ChkCRC16=true;
		int nCount = 510;
        unsigned short chkcrc = 0;
        for (int i=0; i<nCount; i++)chkcrc = mkcrc16(chkcrc, buffer_temp1[i]);
		//ShowMessage(IntToHex(chkcrc,2));
        //MemoX->Lines->Add("CRC:"+IntToHex(chkcrc,4));
        //更新到儲存格
        StringGrid2->Cells[14][31]=IntToHex((chkcrc&0xff),2);//510
        StringGrid2->Cells[15][31]=IntToHex((chkcrc&0xff00)>>8,2);//511
        
        //更新到buffer_temp1[510][511]
        buffer_temp1[510]= chkcrc&0xff;
        buffer_temp1[511]= (chkcrc&0xff00)>>8;
		Txt_Buf[510*2]= getnibbleH((char)(buffer_temp1[510] & 0xff));
        Txt_Buf[510*2+1]= getnibbleL((char)(buffer_temp1[510] & 0xff));

		Txt_Buf[511*2]= getnibbleH((char)(buffer_temp1[511] & 0xff));
        Txt_Buf[511*2+1]= getnibbleL((char)(buffer_temp1[511] & 0xff));

        first_key=0;
		CheckBox1->Checked = false;
	}      
}
//---------------------------------------------------------------------------

void __fastcall TForm1::RadioButton5Click(TObject *Sender)
{
	if (RadioButton5->Checked == true)
    {
        StringGrid2->Cells[2][12] = "04";
        StringGrid2->Cells[3][12] = "B3";
        
    // 直接更新 Txt_Buf 
    Txt_Buf[(12 * 16 * 2) + 2 * 2] = '0'; // 第一個字符
    Txt_Buf[(12 * 16 * 2) + 2 * 2 + 1] = '4'; // 第二個字符
    Txt_Buf[(12 * 16 * 2) + 3 * 2] = 'B'; // 第一個字符
    Txt_Buf[(12 * 16 * 2) + 3 * 2 + 1] = '3'; // 第二個字符
    
	


    //直接更新 buffer_temp1
    buffer_temp1[(12 * 16) + 2] = 0x04; // HEX 碼 0x58
    buffer_temp1[(12 * 16) + 3] = 0xB3; // HEX 碼 0x30
    

    //CRC編碼(檢查)--->BYTE 510 511
       	bool ChkCRC16=true;
		int nCount = 510;
        unsigned short chkcrc = 0;
        for (int i=0; i<nCount; i++)chkcrc = mkcrc16(chkcrc, buffer_temp1[i]);
		//ShowMessage(IntToHex(chkcrc,2));
        //MemoX->Lines->Add("CRC:"+IntToHex(chkcrc,4));
        //更新到儲存格
        StringGrid2->Cells[14][31]=IntToHex((chkcrc&0xff),2);//510
        StringGrid2->Cells[15][31]=IntToHex((chkcrc&0xff00)>>8,2);//511
        
        //更新到buffer_temp1[510][511]
        buffer_temp1[510]= chkcrc&0xff;
        buffer_temp1[511]= (chkcrc&0xff00)>>8;
		Txt_Buf[510*2]= getnibbleH((char)(buffer_temp1[510] & 0xff));
        Txt_Buf[510*2+1]= getnibbleL((char)(buffer_temp1[510] & 0xff));

		Txt_Buf[511*2]= getnibbleH((char)(buffer_temp1[511] & 0xff));
        Txt_Buf[511*2+1]= getnibbleL((char)(buffer_temp1[511] & 0xff));

        first_key=0;
		CheckBox1->Checked = false;
	}        
}
//---------------------------------------------------------------------------

void __fastcall TForm1::RadioButton6Click(TObject *Sender)
{
	if (RadioButton6->Checked == true)
    {
        StringGrid2->Cells[2][12] = "80";
        StringGrid2->Cells[3][12] = "B3";
        
    // 直接更新 Txt_Buf 
    Txt_Buf[(12 * 16 * 2) + 2 * 2] = '8'; // 第一個字符
    Txt_Buf[(12 * 16 * 2) + 2 * 2 + 1] = '0'; // 第二個字符
    Txt_Buf[(12 * 16 * 2) + 3 * 2] = 'B'; // 第一個字符
    Txt_Buf[(12 * 16 * 2) + 3 * 2 + 1] = '3'; // 第二個字符
    
	


    //直接更新 buffer_temp1
    buffer_temp1[(12 * 16) + 2] = 0x80; // HEX 碼 0x80
    buffer_temp1[(12 * 16) + 3] = 0xB3; // HEX 碼 0xB3
    

    //CRC編碼(檢查)--->BYTE 510 511
       	bool ChkCRC16=true;
		int nCount = 510;
        unsigned short chkcrc = 0;
        for (int i=0; i<nCount; i++)chkcrc = mkcrc16(chkcrc, buffer_temp1[i]);
		//ShowMessage(IntToHex(chkcrc,2));
        //MemoX->Lines->Add("CRC:"+IntToHex(chkcrc,4));
        //更新到儲存格
        StringGrid2->Cells[14][31]=IntToHex((chkcrc&0xff),2);//510
        StringGrid2->Cells[15][31]=IntToHex((chkcrc&0xff00)>>8,2);//511
        
        //更新到buffer_temp1[510][511]
        buffer_temp1[510]= chkcrc&0xff;
        buffer_temp1[511]= (chkcrc&0xff00)>>8;
		Txt_Buf[510*2]= getnibbleH((char)(buffer_temp1[510] & 0xff));
        Txt_Buf[510*2+1]= getnibbleL((char)(buffer_temp1[510] & 0xff));

		Txt_Buf[511*2]= getnibbleH((char)(buffer_temp1[511] & 0xff));
        Txt_Buf[511*2+1]= getnibbleL((char)(buffer_temp1[511] & 0xff));

        first_key=0;
		CheckBox1->Checked = false;
	}      
}
//---------------------------------------------------------------------------

void __fastcall TForm1::RadioButton7Click(TObject *Sender)
{
	if (RadioButton7->Checked == true)
    {
        StringGrid2->Cells[2][12] = "8A";
        StringGrid2->Cells[3][12] = "8C";
        
    // 直接更新 Txt_Buf 
    Txt_Buf[(12 * 16 * 2) + 2 * 2] = '8'; // 第一個字符
    Txt_Buf[(12 * 16 * 2) + 2 * 2 + 1] = 'A'; // 第二個字符
    Txt_Buf[(12 * 16 * 2) + 3 * 2] = '8'; // 第一個字符
    Txt_Buf[(12 * 16 * 2) + 3 * 2 + 1] = 'C'; // 第二個字符
    
	


    //直接更新 buffer_temp1
    buffer_temp1[(12 * 16) + 2] = 0x8A; // HEX 碼 0x8A
    buffer_temp1[(12 * 16) + 3] = 0x8C; // HEX 碼 0x8C
    

    //CRC編碼(檢查)--->BYTE 510 511
       	bool ChkCRC16=true;
		int nCount = 510;
        unsigned short chkcrc = 0;
        for (int i=0; i<nCount; i++)chkcrc = mkcrc16(chkcrc, buffer_temp1[i]);
		//ShowMessage(IntToHex(chkcrc,2));
        //MemoX->Lines->Add("CRC:"+IntToHex(chkcrc,4));
        //更新到儲存格
        StringGrid2->Cells[14][31]=IntToHex((chkcrc&0xff),2);//510
        StringGrid2->Cells[15][31]=IntToHex((chkcrc&0xff00)>>8,2);//511
        
        //更新到buffer_temp1[510][511]
        buffer_temp1[510]= chkcrc&0xff;
        buffer_temp1[511]= (chkcrc&0xff00)>>8;
		Txt_Buf[510*2]= getnibbleH((char)(buffer_temp1[510] & 0xff));
        Txt_Buf[510*2+1]= getnibbleL((char)(buffer_temp1[510] & 0xff));

		Txt_Buf[511*2]= getnibbleH((char)(buffer_temp1[511] & 0xff));
        Txt_Buf[511*2+1]= getnibbleL((char)(buffer_temp1[511] & 0xff));

        first_key=0;
		CheckBox1->Checked = false;
	}             
}
//---------------------------------------------------------------------------

void __fastcall TForm1::CheckBox1Click(TObject *Sender)
{
	if (CheckBox1->Checked == true)
    {
        StringGrid2->Cells[2][12] = "00";
        StringGrid2->Cells[3][12] = "00";
        
    // 直接更新 Txt_Buf 
    Txt_Buf[(12 * 16 * 2) + 2 * 2] = '0'; // 第一個字符
    Txt_Buf[(12 * 16 * 2) + 2 * 2 + 1] = '0'; // 第二個字符
    Txt_Buf[(12 * 16 * 2) + 3 * 2] = '0'; // 第一個字符
    Txt_Buf[(12 * 16 * 2) + 3 * 2 + 1] = '0'; // 第二個字符
    
	


    //直接更新 buffer_temp1
    buffer_temp1[(12 * 16) + 2] = 0x00; // HEX 碼 0x00
    buffer_temp1[(12 * 16) + 3] = 0x00; // HEX 碼 0x00
    

    //CRC編碼(檢查)--->BYTE 510 511
       	bool ChkCRC16=true;
		int nCount = 510;
        unsigned short chkcrc = 0;
        for (int i=0; i<nCount; i++)chkcrc = mkcrc16(chkcrc, buffer_temp1[i]);
		//ShowMessage(IntToHex(chkcrc,2));
        //MemoX->Lines->Add("CRC:"+IntToHex(chkcrc,4));
        //更新到儲存格
        StringGrid2->Cells[14][31]=IntToHex((chkcrc&0xff),2);//510
        StringGrid2->Cells[15][31]=IntToHex((chkcrc&0xff00)>>8,2);//511
        
        //更新到buffer_temp1[510][511]
        buffer_temp1[510]= chkcrc&0xff;
        buffer_temp1[511]= (chkcrc&0xff00)>>8;
		Txt_Buf[510*2]= getnibbleH((char)(buffer_temp1[510] & 0xff));
        Txt_Buf[510*2+1]= getnibbleL((char)(buffer_temp1[510] & 0xff));

		Txt_Buf[511*2]= getnibbleH((char)(buffer_temp1[511] & 0xff));
        Txt_Buf[511*2+1]= getnibbleL((char)(buffer_temp1[511] & 0xff));

        first_key=0;

		RadioButton4->Checked = false;
		RadioButton5->Checked = false;
		RadioButton6->Checked = false;
		RadioButton7->Checked = false;
	}             
}
//---------------------------------------------------------------------------



void __fastcall TForm1::RadioButton8Click(TObject *Sender)
{
	if (RadioButton8->Checked == true)
    {
        StringGrid2->Cells[6][12] = "86";
        StringGrid2->Cells[7][12] = "32";
        
    // 直接更新 Txt_Buf 
    Txt_Buf[(12 * 16 * 2) + 6 * 2] = '8'; // 第一個字符
    Txt_Buf[(12 * 16 * 2) + 6 * 2 + 1] = '6'; // 第二個字符
    Txt_Buf[(12 * 16 * 2) + 7 * 2] = '3'; // 第一個字符
    Txt_Buf[(12 * 16 * 2) + 7 * 2 + 1] = '2'; // 第二個字符
    
	


    //直接更新 buffer_temp1
    buffer_temp1[(12 * 16) + 6] = 0x86; // HEX 碼 0x86
    buffer_temp1[(12 * 16) + 7] = 0x32; // HEX 碼 0x32
    

    //CRC編碼(檢查)--->BYTE 510 511
       	bool ChkCRC16=true;
		int nCount = 510;
        unsigned short chkcrc = 0;
        for (int i=0; i<nCount; i++)chkcrc = mkcrc16(chkcrc, buffer_temp1[i]);
		//ShowMessage(IntToHex(chkcrc,2));
        //MemoX->Lines->Add("CRC:"+IntToHex(chkcrc,4));
        //更新到儲存格
        StringGrid2->Cells[14][31]=IntToHex((chkcrc&0xff),2);//510
        StringGrid2->Cells[15][31]=IntToHex((chkcrc&0xff00)>>8,2);//511
        
        //更新到buffer_temp1[510][511]
        buffer_temp1[510]= chkcrc&0xff;
        buffer_temp1[511]= (chkcrc&0xff00)>>8;
		Txt_Buf[510*2]= getnibbleH((char)(buffer_temp1[510] & 0xff));
        Txt_Buf[510*2+1]= getnibbleL((char)(buffer_temp1[510] & 0xff));

		Txt_Buf[511*2]= getnibbleH((char)(buffer_temp1[511] & 0xff));
        Txt_Buf[511*2+1]= getnibbleL((char)(buffer_temp1[511] & 0xff));

        first_key=0;
		CheckBox2->Checked = false;
	}      
}
//---------------------------------------------------------------------------

void __fastcall TForm1::RadioButton9Click(TObject *Sender)
{
    if (RadioButton9->Checked == true)
    {
        StringGrid2->Cells[6][12] = "04";
        StringGrid2->Cells[7][12] = "B3";
        
    // 直接更新 Txt_Buf 
    Txt_Buf[(12 * 16 * 2) + 6 * 2] = '0'; // 第一個字符
    Txt_Buf[(12 * 16 * 2) + 6 * 2 + 1] = '4'; // 第二個字符
    Txt_Buf[(12 * 16 * 2) + 7 * 2] = 'B'; // 第一個字符
    Txt_Buf[(12 * 16 * 2) + 7 * 2 + 1] = '3'; // 第二個字符
    
	


    //直接更新 buffer_temp1
    buffer_temp1[(12 * 16) + 6] = 0x04; // HEX 碼 0x04
    buffer_temp1[(12 * 16) + 7] = 0xB3; // HEX 碼 0xB3
    

    //CRC編碼(檢查)--->BYTE 510 511
       	bool ChkCRC16=true;
		int nCount = 510;
        unsigned short chkcrc = 0;
        for (int i=0; i<nCount; i++)chkcrc = mkcrc16(chkcrc, buffer_temp1[i]);
		//ShowMessage(IntToHex(chkcrc,2));
        //MemoX->Lines->Add("CRC:"+IntToHex(chkcrc,4));
        //更新到儲存格
        StringGrid2->Cells[14][31]=IntToHex((chkcrc&0xff),2);//510
        StringGrid2->Cells[15][31]=IntToHex((chkcrc&0xff00)>>8,2);//511
        
        //更新到buffer_temp1[510][511]
        buffer_temp1[510]= chkcrc&0xff;
        buffer_temp1[511]= (chkcrc&0xff00)>>8;
		Txt_Buf[510*2]= getnibbleH((char)(buffer_temp1[510] & 0xff));
        Txt_Buf[510*2+1]= getnibbleL((char)(buffer_temp1[510] & 0xff));

		Txt_Buf[511*2]= getnibbleH((char)(buffer_temp1[511] & 0xff));
        Txt_Buf[511*2+1]= getnibbleL((char)(buffer_temp1[511] & 0xff));

        first_key=0;
		CheckBox2->Checked = false;
	}            
}
//---------------------------------------------------------------------------
void __fastcall TForm1::RadioButton10Click(TObject *Sender)
{
	if (RadioButton10->Checked == true)
    {
        StringGrid2->Cells[6][12] = "80";
        StringGrid2->Cells[7][12] = "B3";
        
    // 直接更新 Txt_Buf 
    Txt_Buf[(12 * 16 * 2) + 6 * 2] = '8'; // 第一個字符
    Txt_Buf[(12 * 16 * 2) + 6 * 2 + 1] = '0'; // 第二個字符
    Txt_Buf[(12 * 16 * 2) + 7 * 2] = 'B'; // 第一個字符
    Txt_Buf[(12 * 16 * 2) + 7 * 2 + 1] = '3'; // 第二個字符
    
	


    //直接更新 buffer_temp1
    buffer_temp1[(12 * 16) + 6] = 0x80; // HEX 碼 0x80
    buffer_temp1[(12 * 16) + 7] = 0xB3; // HEX 碼 0xB3
    

    //CRC編碼(檢查)--->BYTE 510 511
       	bool ChkCRC16=true;
		int nCount = 510;
        unsigned short chkcrc = 0;
        for (int i=0; i<nCount; i++)chkcrc = mkcrc16(chkcrc, buffer_temp1[i]);
		//ShowMessage(IntToHex(chkcrc,2));
        //MemoX->Lines->Add("CRC:"+IntToHex(chkcrc,4));
        //更新到儲存格
        StringGrid2->Cells[14][31]=IntToHex((chkcrc&0xff),2);//510
        StringGrid2->Cells[15][31]=IntToHex((chkcrc&0xff00)>>8,2);//511
        
        //更新到buffer_temp1[510][511]
        buffer_temp1[510]= chkcrc&0xff;
        buffer_temp1[511]= (chkcrc&0xff00)>>8;
		Txt_Buf[510*2]= getnibbleH((char)(buffer_temp1[510] & 0xff));
        Txt_Buf[510*2+1]= getnibbleL((char)(buffer_temp1[510] & 0xff));

		Txt_Buf[511*2]= getnibbleH((char)(buffer_temp1[511] & 0xff));
        Txt_Buf[511*2+1]= getnibbleL((char)(buffer_temp1[511] & 0xff));

        first_key=0;
		CheckBox2->Checked = false;
	}           
}
//-------------------------------------------------------------------------
void __fastcall TForm1::RadioButton11Click(TObject *Sender)
{
	if (RadioButton11->Checked == true)
    {
        StringGrid2->Cells[6][12] = "8A";
        StringGrid2->Cells[7][12] = "8C";
        
    // 直接更新 Txt_Buf 
    Txt_Buf[(12 * 16 * 2) + 6 * 2] = '8'; // 第一個字符
    Txt_Buf[(12 * 16 * 2) + 6 * 2 + 1] = 'A'; // 第二個字符
    Txt_Buf[(12 * 16 * 2) + 7 * 2] = '8'; // 第一個字符
    Txt_Buf[(12 * 16 * 2) + 7 * 2 + 1] = 'C'; // 第二個字符
    
	


    //直接更新 buffer_temp1
    buffer_temp1[(12 * 16) + 6] = 0x8A; // HEX 碼 0x8A
    buffer_temp1[(12 * 16) + 7] = 0x8C; // HEX 碼 0x8C
    

    //CRC編碼(檢查)--->BYTE 510 511
       	bool ChkCRC16=true;
		int nCount = 510;
        unsigned short chkcrc = 0;
        for (int i=0; i<nCount; i++)chkcrc = mkcrc16(chkcrc, buffer_temp1[i]);
		//ShowMessage(IntToHex(chkcrc,2));
        //MemoX->Lines->Add("CRC:"+IntToHex(chkcrc,4));
        //更新到儲存格
        StringGrid2->Cells[14][31]=IntToHex((chkcrc&0xff),2);//510
        StringGrid2->Cells[15][31]=IntToHex((chkcrc&0xff00)>>8,2);//511
        
        //更新到buffer_temp1[510][511]
        buffer_temp1[510]= chkcrc&0xff;
        buffer_temp1[511]= (chkcrc&0xff00)>>8;
		Txt_Buf[510*2]= getnibbleH((char)(buffer_temp1[510] & 0xff));
        Txt_Buf[510*2+1]= getnibbleL((char)(buffer_temp1[510] & 0xff));

		Txt_Buf[511*2]= getnibbleH((char)(buffer_temp1[511] & 0xff));
        Txt_Buf[511*2+1]= getnibbleL((char)(buffer_temp1[511] & 0xff));

        first_key=0;
		CheckBox2->Checked = false;
	}                  
}
//---------------------------------------------------------------------------

void __fastcall TForm1::CheckBox2Click(TObject *Sender)
{
	if (CheckBox2->Checked == true)
    {
        StringGrid2->Cells[6][12] = "00";
        StringGrid2->Cells[7][12] = "00";
        
    // 直接更新 Txt_Buf 
    Txt_Buf[(12 * 16 * 2) + 6 * 2] = '0'; // 第一個字符
    Txt_Buf[(12 * 16 * 2) + 6 * 2 + 1] = '0'; // 第二個字符
    Txt_Buf[(12 * 16 * 2) + 7 * 2] = '0'; // 第一個字符
    Txt_Buf[(12 * 16 * 2) + 7 * 2 + 1] = '0'; // 第二個字符
    
	


    //直接更新 buffer_temp1
    buffer_temp1[(12 * 16) + 6] = 0x00; // HEX 碼 0x00
    buffer_temp1[(12 * 16) + 7] = 0x00; // HEX 碼 0x00
    

    //CRC編碼(檢查)--->BYTE 510 511
       	bool ChkCRC16=true;
		int nCount = 510;
        unsigned short chkcrc = 0;
        for (int i=0; i<nCount; i++)chkcrc = mkcrc16(chkcrc, buffer_temp1[i]);
		//ShowMessage(IntToHex(chkcrc,2));
        //MemoX->Lines->Add("CRC:"+IntToHex(chkcrc,4));
        //更新到儲存格
        StringGrid2->Cells[14][31]=IntToHex((chkcrc&0xff),2);//510
        StringGrid2->Cells[15][31]=IntToHex((chkcrc&0xff00)>>8,2);//511
        
        //更新到buffer_temp1[510][511]
        buffer_temp1[510]= chkcrc&0xff;
        buffer_temp1[511]= (chkcrc&0xff00)>>8;
		Txt_Buf[510*2]= getnibbleH((char)(buffer_temp1[510] & 0xff));
        Txt_Buf[510*2+1]= getnibbleL((char)(buffer_temp1[510] & 0xff));

		Txt_Buf[511*2]= getnibbleH((char)(buffer_temp1[511] & 0xff));
        Txt_Buf[511*2+1]= getnibbleL((char)(buffer_temp1[511] & 0xff));

        first_key=0;

		RadioButton8->Checked = false;
		RadioButton9->Checked = false;
		RadioButton10->Checked = false;
		RadioButton11->Checked = false;
	}          
}
//---------------------------------------------------------------------------



void __fastcall TForm1::RadioButton12Click(TObject *Sender)
{
	if (RadioButton12->Checked == true)
    {
        StringGrid2->Cells[10][2] = "C3";
        StringGrid2->Cells[11][2] = "00";
        StringGrid2->Cells[12][2] = "82";
        StringGrid2->Cells[13][2] = "00";
		StringGrid2->Cells[14][2] = "73";
        StringGrid2->Cells[15][2] = "00";

		// 直接更新 Txt_Buf 
    Txt_Buf[(2 * 16 * 2) + 10 * 2] = 'C'; // 第一個字符
    Txt_Buf[(2 * 16 * 2) + 10 * 2 + 1] = '3'; // 第二個字符
    Txt_Buf[(2 * 16 * 2) + 11 * 2] = '0'; // 第一個字符
    Txt_Buf[(2 * 16 * 2) + 11 * 2 + 1] = '0'; // 第二個字符
    Txt_Buf[(2 * 16 * 2) + 12 * 2] = '8'; // 第一個字符
    Txt_Buf[(2 * 16 * 2) + 12 * 2 + 1] = '2'; // 第二個字符
    Txt_Buf[(2 * 16 * 2) + 13 * 2] = '0'; // 第一個字符
    Txt_Buf[(2 * 16 * 2) + 13 * 2 + 1] = '0'; // 第二個字符
    Txt_Buf[(2 * 16 * 2) + 14 * 2] = '7'; // 第一個字符
    Txt_Buf[(2 * 16 * 2) + 14 * 2 + 1] = '3'; // 第二個字符
    Txt_Buf[(2 * 16 * 2) + 15 * 2] = '0'; // 第一個字符
    Txt_Buf[(2 * 16 * 2) + 15 * 2 + 1] = '0'; // 第二個字符
    
    
	


    //直接更新 buffer_temp1
    buffer_temp1[(2 * 16) + 10] = 0xC3; // HEX 碼 0xC3
    buffer_temp1[(2 * 16) + 11] = 0x00; // HEX 碼 0x00
    buffer_temp1[(2 * 16) + 12] = 0x82; // HEX 碼 0x82
    buffer_temp1[(2 * 16) + 13] = 0x00; // HEX 碼 0x00
    buffer_temp1[(2 * 16) + 14] = 0x73; // HEX 碼 0x73
    buffer_temp1[(2 * 16) + 15] = 0x00; // HEX 碼 0x00
    

    //CRC編碼(檢查)--->BYTE 510 511
       	bool ChkCRC16=true;
		int nCount = 510;
        unsigned short chkcrc = 0;
        for (int i=0; i<nCount; i++)chkcrc = mkcrc16(chkcrc, buffer_temp1[i]);
		//ShowMessage(IntToHex(chkcrc,2));
        //MemoX->Lines->Add("CRC:"+IntToHex(chkcrc,4));
        //更新到儲存格
        StringGrid2->Cells[14][31]=IntToHex((chkcrc&0xff),2);//510
        StringGrid2->Cells[15][31]=IntToHex((chkcrc&0xff00)>>8,2);//511
        
        //更新到buffer_temp1[510][511]
        buffer_temp1[510]= chkcrc&0xff;
        buffer_temp1[511]= (chkcrc&0xff00)>>8;
		Txt_Buf[510*2]= getnibbleH((char)(buffer_temp1[510] & 0xff));
        Txt_Buf[510*2+1]= getnibbleL((char)(buffer_temp1[510] & 0xff));

		Txt_Buf[511*2]= getnibbleH((char)(buffer_temp1[511] & 0xff));
        Txt_Buf[511*2+1]= getnibbleL((char)(buffer_temp1[511] & 0xff));

        first_key=0;
		CheckBox3->Checked = false;
	}                          
}
//---------------------------------------------------------------------------

void __fastcall TForm1::RadioButton13Click(TObject *Sender)
{
	if (RadioButton13->Checked == true)
    {
        StringGrid2->Cells[10][2] = "27";
        StringGrid2->Cells[11][2] = "01";
        StringGrid2->Cells[12][2] = "A0";
        StringGrid2->Cells[13][2] = "00";
		StringGrid2->Cells[14][2] = "82";
        StringGrid2->Cells[15][2] = "00";

		// 直接更新 Txt_Buf 
    Txt_Buf[(2 * 16 * 2) + 10 * 2] = '2'; // 第一個字符
    Txt_Buf[(2 * 16 * 2) + 10 * 2 + 1] = '7'; // 第二個字符
    Txt_Buf[(2 * 16 * 2) + 11 * 2] = '0'; // 第一個字符
    Txt_Buf[(2 * 16 * 2) + 11 * 2 + 1] = '1'; // 第二個字符
    Txt_Buf[(2 * 16 * 2) + 12 * 2] = 'A'; // 第一個字符
    Txt_Buf[(2 * 16 * 2) + 12 * 2 + 1] = '0'; // 第二個字符
    Txt_Buf[(2 * 16 * 2) + 13 * 2] = '0'; // 第一個字符
    Txt_Buf[(2 * 16 * 2) + 13 * 2 + 1] = '0'; // 第二個字符
    Txt_Buf[(2 * 16 * 2) + 14 * 2] = '8'; // 第一個字符
    Txt_Buf[(2 * 16 * 2) + 14 * 2 + 1] = '2'; // 第二個字符
    Txt_Buf[(2 * 16 * 2) + 15 * 2] = '0'; // 第一個字符
    Txt_Buf[(2 * 16 * 2) + 15 * 2 + 1] = '0'; // 第二個字符
    
    
	


    //直接更新 buffer_temp1
    buffer_temp1[(2 * 16) + 10] = 0x27; // HEX 碼 0x27
    buffer_temp1[(2 * 16) + 11] = 0x01; // HEX 碼 0x01
    buffer_temp1[(2 * 16) + 12] = 0xA0; // HEX 碼 0xA0
    buffer_temp1[(2 * 16) + 13] = 0x00; // HEX 碼 0x00
    buffer_temp1[(2 * 16) + 14] = 0x82; // HEX 碼 0x82
    buffer_temp1[(2 * 16) + 15] = 0x00; // HEX 碼 0x00
    

    //CRC編碼(檢查)--->BYTE 510 511
       	bool ChkCRC16=true;
		int nCount = 510;
        unsigned short chkcrc = 0;
        for (int i=0; i<nCount; i++)chkcrc = mkcrc16(chkcrc, buffer_temp1[i]);
		//ShowMessage(IntToHex(chkcrc,2));
        //MemoX->Lines->Add("CRC:"+IntToHex(chkcrc,4));
        //更新到儲存格
        StringGrid2->Cells[14][31]=IntToHex((chkcrc&0xff),2);//510
        StringGrid2->Cells[15][31]=IntToHex((chkcrc&0xff00)>>8,2);//511
        
        //更新到buffer_temp1[510][511]
        buffer_temp1[510]= chkcrc&0xff;
        buffer_temp1[511]= (chkcrc&0xff00)>>8;
		Txt_Buf[510*2]= getnibbleH((char)(buffer_temp1[510] & 0xff));
        Txt_Buf[510*2+1]= getnibbleL((char)(buffer_temp1[510] & 0xff));

		Txt_Buf[511*2]= getnibbleH((char)(buffer_temp1[511] & 0xff));
        Txt_Buf[511*2+1]= getnibbleL((char)(buffer_temp1[511] & 0xff));

        first_key=0;
		CheckBox3->Checked = false;
	}       
}
//---------------------------------------------------------------------------

void __fastcall TForm1::RadioButton14Click(TObject *Sender)
{
	if (RadioButton14->Checked == true)
    {
        StringGrid2->Cells[10][2] = "9A";
        StringGrid2->Cells[11][2] = "01";
        StringGrid2->Cells[12][2] = "DC";
        StringGrid2->Cells[13][2] = "00";
		StringGrid2->Cells[14][2] = "BE";
        StringGrid2->Cells[15][2] = "00";

		// 直接更新 Txt_Buf 
    Txt_Buf[(2 * 16 * 2) + 10 * 2] = '9'; // 第一個字符
    Txt_Buf[(2 * 16 * 2) + 10 * 2 + 1] = 'A'; // 第二個字符
    Txt_Buf[(2 * 16 * 2) + 11 * 2] = '0'; // 第一個字符
    Txt_Buf[(2 * 16 * 2) + 11 * 2 + 1] = '1'; // 第二個字符
    Txt_Buf[(2 * 16 * 2) + 12 * 2] = 'D'; // 第一個字符
    Txt_Buf[(2 * 16 * 2) + 12 * 2 + 1] = 'C'; // 第二個字符
    Txt_Buf[(2 * 16 * 2) + 13 * 2] = '0'; // 第一個字符
    Txt_Buf[(2 * 16 * 2) + 13 * 2 + 1] = '0'; // 第二個字符
    Txt_Buf[(2 * 16 * 2) + 14 * 2] = 'B'; // 第一個字符
    Txt_Buf[(2 * 16 * 2) + 14 * 2 + 1] = 'E'; // 第二個字符
    Txt_Buf[(2 * 16 * 2) + 15 * 2] = '0'; // 第一個字符
    Txt_Buf[(2 * 16 * 2) + 15 * 2 + 1] = '0'; // 第二個字符
    
    
	


    //直接更新 buffer_temp1
    buffer_temp1[(2 * 16) + 10] = 0x9A; // HEX 碼 0x9A
    buffer_temp1[(2 * 16) + 11] = 0x01; // HEX 碼 0x01
    buffer_temp1[(2 * 16) + 12] = 0xDC; // HEX 碼 0xDC
    buffer_temp1[(2 * 16) + 13] = 0x00; // HEX 碼 0x00
    buffer_temp1[(2 * 16) + 14] = 0xBE; // HEX 碼 0xBE
    buffer_temp1[(2 * 16) + 15] = 0x00; // HEX 碼 0x00
    

    //CRC編碼(檢查)--->BYTE 510 511
       	bool ChkCRC16=true;
		int nCount = 510;
        unsigned short chkcrc = 0;
        for (int i=0; i<nCount; i++)chkcrc = mkcrc16(chkcrc, buffer_temp1[i]);
		//ShowMessage(IntToHex(chkcrc,2));
        //MemoX->Lines->Add("CRC:"+IntToHex(chkcrc,4));
        //更新到儲存格
        StringGrid2->Cells[14][31]=IntToHex((chkcrc&0xff),2);//510
        StringGrid2->Cells[15][31]=IntToHex((chkcrc&0xff00)>>8,2);//511
        
        //更新到buffer_temp1[510][511]
        buffer_temp1[510]= chkcrc&0xff;
        buffer_temp1[511]= (chkcrc&0xff00)>>8;
		Txt_Buf[510*2]= getnibbleH((char)(buffer_temp1[510] & 0xff));
        Txt_Buf[510*2+1]= getnibbleL((char)(buffer_temp1[510] & 0xff));

		Txt_Buf[511*2]= getnibbleH((char)(buffer_temp1[511] & 0xff));
        Txt_Buf[511*2+1]= getnibbleL((char)(buffer_temp1[511] & 0xff));

        first_key=0;
		CheckBox3->Checked = false;
	}       
}
//---------------------------------------------------------------------------

void __fastcall TForm1::RadioButton15Click(TObject *Sender)
{
	if (RadioButton15->Checked == true)
    {
        StringGrid2->Cells[10][2] = "9A";
        StringGrid2->Cells[11][2] = "01";
        StringGrid2->Cells[12][2] = "DC";
        StringGrid2->Cells[13][2] = "00";
		StringGrid2->Cells[14][2] = "BE";
        StringGrid2->Cells[15][2] = "00";

		// 直接更新 Txt_Buf 
    Txt_Buf[(2 * 16 * 2) + 10 * 2] = '9'; // 第一個字符
    Txt_Buf[(2 * 16 * 2) + 10 * 2 + 1] = 'A'; // 第二個字符
    Txt_Buf[(2 * 16 * 2) + 11 * 2] = '0'; // 第一個字符
    Txt_Buf[(2 * 16 * 2) + 11 * 2 + 1] = '1'; // 第二個字符
    Txt_Buf[(2 * 16 * 2) + 12 * 2] = 'D'; // 第一個字符
    Txt_Buf[(2 * 16 * 2) + 12 * 2 + 1] = 'C'; // 第二個字符
    Txt_Buf[(2 * 16 * 2) + 13 * 2] = '0'; // 第一個字符
    Txt_Buf[(2 * 16 * 2) + 13 * 2 + 1] = '0'; // 第二個字符
    Txt_Buf[(2 * 16 * 2) + 14 * 2] = 'B'; // 第一個字符
    Txt_Buf[(2 * 16 * 2) + 14 * 2 + 1] = 'E'; // 第二個字符
    Txt_Buf[(2 * 16 * 2) + 15 * 2] = '0'; // 第一個字符
    Txt_Buf[(2 * 16 * 2) + 15 * 2 + 1] = '0'; // 第二個字符
    
    
	


    //直接更新 buffer_temp1
    buffer_temp1[(2 * 16) + 10] = 0x9A; // HEX 碼 0x9A
    buffer_temp1[(2 * 16) + 11] = 0x01; // HEX 碼 0x01
    buffer_temp1[(2 * 16) + 12] = 0xDC; // HEX 碼 0xDC
    buffer_temp1[(2 * 16) + 13] = 0x00; // HEX 碼 0x00
    buffer_temp1[(2 * 16) + 14] = 0xBE; // HEX 碼 0xBE
    buffer_temp1[(2 * 16) + 15] = 0x00; // HEX 碼 0x00
    

    //CRC編碼(檢查)--->BYTE 510 511
       	bool ChkCRC16=true;
		int nCount = 510;
        unsigned short chkcrc = 0;
        for (int i=0; i<nCount; i++)chkcrc = mkcrc16(chkcrc, buffer_temp1[i]);
		//ShowMessage(IntToHex(chkcrc,2));
        //MemoX->Lines->Add("CRC:"+IntToHex(chkcrc,4));
        //更新到儲存格
        StringGrid2->Cells[14][31]=IntToHex((chkcrc&0xff),2);//510
        StringGrid2->Cells[15][31]=IntToHex((chkcrc&0xff00)>>8,2);//511
        
        //更新到buffer_temp1[510][511]
        buffer_temp1[510]= chkcrc&0xff;
        buffer_temp1[511]= (chkcrc&0xff00)>>8;
		Txt_Buf[510*2]= getnibbleH((char)(buffer_temp1[510] & 0xff));
        Txt_Buf[510*2+1]= getnibbleL((char)(buffer_temp1[510] & 0xff));

		Txt_Buf[511*2]= getnibbleH((char)(buffer_temp1[511] & 0xff));
        Txt_Buf[511*2+1]= getnibbleL((char)(buffer_temp1[511] & 0xff));

        first_key=0;
		CheckBox3->Checked = false;
	}                     
}
//---------------------------------------------------------------------------

void __fastcall TForm1::CheckBox3Click(TObject *Sender)
{
	if (CheckBox3->Checked == true)
    {
        StringGrid2->Cells[10][2] = "00";
        StringGrid2->Cells[11][2] = "00";
        StringGrid2->Cells[12][2] = "00";
        StringGrid2->Cells[13][2] = "00";
		StringGrid2->Cells[14][2] = "00";
        StringGrid2->Cells[15][2] = "00";

		// 直接更新 Txt_Buf 
    Txt_Buf[(2 * 16 * 2) + 10 * 2] = '0'; // 第一個字符
    Txt_Buf[(2 * 16 * 2) + 10 * 2 + 1] = '0'; // 第二個字符
    Txt_Buf[(2 * 16 * 2) + 11 * 2] = '0'; // 第一個字符
    Txt_Buf[(2 * 16 * 2) + 11 * 2 + 1] = '0'; // 第二個字符
    Txt_Buf[(2 * 16 * 2) + 12 * 2] = '0'; // 第一個字符
    Txt_Buf[(2 * 16 * 2) + 12 * 2 + 1] = '0'; // 第二個字符
    Txt_Buf[(2 * 16 * 2) + 13 * 2] = '0'; // 第一個字符
    Txt_Buf[(2 * 16 * 2) + 13 * 2 + 1] = '0'; // 第二個字符
    Txt_Buf[(2 * 16 * 2) + 14 * 2] = '0'; // 第一個字符
    Txt_Buf[(2 * 16 * 2) + 14 * 2 + 1] = '0'; // 第二個字符
    Txt_Buf[(2 * 16 * 2) + 15 * 2] = '0'; // 第一個字符
    Txt_Buf[(2 * 16 * 2) + 15 * 2 + 1] = '0'; // 第二個字符
    
    
	


    //直接更新 buffer_temp1
    buffer_temp1[(2 * 16) + 10] = 0x00; // HEX 碼 0x00
    buffer_temp1[(2 * 16) + 11] = 0x00; // HEX 碼 0x00
    buffer_temp1[(2 * 16) + 12] = 0x00; // HEX 碼 0x00
    buffer_temp1[(2 * 16) + 13] = 0x00; // HEX 碼 0x00
    buffer_temp1[(2 * 16) + 14] = 0x00; // HEX 碼 0x00
    buffer_temp1[(2 * 16) + 15] = 0x00; // HEX 碼 0x00
    

    //CRC編碼(檢查)--->BYTE 510 511
       	bool ChkCRC16=true;
		int nCount = 510;
        unsigned short chkcrc = 0;
        for (int i=0; i<nCount; i++)chkcrc = mkcrc16(chkcrc, buffer_temp1[i]);
		//ShowMessage(IntToHex(chkcrc,2));
        //MemoX->Lines->Add("CRC:"+IntToHex(chkcrc,4));
        //更新到儲存格
        StringGrid2->Cells[14][31]=IntToHex((chkcrc&0xff),2);//510
        StringGrid2->Cells[15][31]=IntToHex((chkcrc&0xff00)>>8,2);//511
        
        //更新到buffer_temp1[510][511]
        buffer_temp1[510]= chkcrc&0xff;
        buffer_temp1[511]= (chkcrc&0xff00)>>8;
		Txt_Buf[510*2]= getnibbleH((char)(buffer_temp1[510] & 0xff));
        Txt_Buf[510*2+1]= getnibbleL((char)(buffer_temp1[510] & 0xff));

		Txt_Buf[511*2]= getnibbleH((char)(buffer_temp1[511] & 0xff));
        Txt_Buf[511*2+1]= getnibbleL((char)(buffer_temp1[511] & 0xff));

        first_key=0;
		RadioButton12->Checked = false;
		RadioButton13->Checked = false;
		RadioButton14->Checked = false;
		RadioButton15->Checked = false;
	}           
}
//---------------------------------------------------------------------------

void __fastcall TForm1::CheckBox4Click(TObject *Sender)
{
    RadioButton1->Checked = false;
	RadioButton2->Checked = false;
	RadioButton3->Checked = false;  
}
//---------------------------------------------------------------------------

void __fastcall TForm1::RadioButton1Click(TObject *Sender)
{
    RadioButton1->Checked = true; 
    CheckBox4->Checked = false;       
}
//---------------------------------------------------------------------------

void __fastcall TForm1::RadioButton2Click(TObject *Sender)
{
    RadioButton2->Checked = true; 
    CheckBox4->Checked = false;       
}
//---------------------------------------------------------------------------
