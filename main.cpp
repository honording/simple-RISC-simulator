//EECS 643 Advanced Computer Organization
//Fall 2012
//Course Porject
//12/14/2012
//By Hongyuan Ding


#include <iostream>
#include <fstream>
#include <string>
#include <stdlib.h>

using namespace std;

#define Max_reg				32
#define Max_mem				125
#define Max_inst			100
#define Max_label			100
#define Pipeline_reg_num	8
#define No_value			-32768

int Register[Max_reg][2]; //[][0]value, [][1] status, 0 available ohter unavailable
int Memory[Max_mem]; //
struct Sinstruction {
	string op; //DADD, SUB, LD/SD, BNEZ
	char type; //I/R
	int rs;
	int rt;
	int rd;
	int immediate;
	string jump; //temp store jump label name
} Instruction[Max_inst];
struct Slabel {
	string name; //label name
	int inst_add; //related instruction address in Instruction[]
} Label[Max_label];
struct Spipeline_reg {
	int status; //0 empty, 1 occupied, 2 stall
	int Reg_num; //output register for forwarding
	int Reg_value;
	int Reg_cycle;
	int rs_value;
	int rs_cycle;
	int rt_value;
	int rt_cycle;
	int Inst_num; //Pipeline total instruction number
	Sinstruction Instruction;
} Pipeline_reg[Pipeline_reg_num];
//0 IF1, 1 IF2, 2 ID, 3 EXE, 4 M1, 5 M2, 6 M3, 7 WB
//define global variable
bool is_flushed;
int PC;
int PC_target; //handle flushed PC
bool PC_updated;
int cycle;
int inst_add;
int inst_num;
int num_label;
ofstream fout;

void init() { //initialize
	int i;
	for (i=0;i<Max_reg;i++) {
		Register[i][0]=0;
		Register[i][1]=0; //@ available 
	}
	for (i=0;i<Max_mem;i++)
		Memory[i]=0;
	for (i=0;i<Max_inst;i++) {
		Instruction[i].op="";
		Instruction[i].type=' ';
		Instruction[i].rs=No_value;
		Instruction[i].rt=No_value;
		Instruction[i].rd=No_value;
		Instruction[i].immediate=No_value;
		Instruction[i].jump="";
	}
	for (i=0;i<Pipeline_reg_num;i++) {
		Pipeline_reg[i].status=0;
		Pipeline_reg[i].Reg_num=No_value;
		Pipeline_reg[i].Reg_value=No_value;
		Pipeline_reg[i].Reg_cycle=No_value;
		Pipeline_reg[i].rs_value=No_value;
		Pipeline_reg[i].rs_cycle=No_value;
		Pipeline_reg[i].rt_value=No_value;
		Pipeline_reg[i].rt_cycle=No_value;
		Pipeline_reg[i].Inst_num=0;
		Pipeline_reg[i].Instruction=Instruction[0];
	}
	for (i=0;i<Max_label;i++) {
		Label[i].name="";
		Label[i].inst_add=No_value;
	}
}

int string2int(string str) {
	int n; //length
	int v; //value
	int i;
	int indicator;
	indicator=1;
	v=0;
	n=str.length();
	if (n == 0) return 0; 
	else {
		for (i=0;i<n;i++)
			if (str[i] == '-') indicator=-1;
			else v=v*10+(str[i]-48);
		return v*indicator;
	}	
}

bool analyze_src(string sinput) {
	ifstream fin; //input file
	string line; //read line
	string t; //temp
	int reg_num; //register number
	int reg_value; //register value
	int mem_add;
	int mem_value;
	int i,l; //temp, length
	int head,rear;
	int rs,rt,rd,immediate;
	bool type_rs,type_rt; //true for R-type, false for I-type
	char type; //I-type, R-type
	string jump;
	head=0;
	rear=0;
	line="";
	
	cout<<"Start to read source file."<<endl;
	fin.open(sinput.c_str());
	if (fin.is_open()) { //contain two statements: while and fin.close()
		while (fin.good() && !fin.eof())	
			if ((line=="REGISTER") || (line=="REGISTERS")) {
				getline(fin,line);
				if (line[line.length()-1]=='\r') line=line.substr(0,line.length()-1);
				while ((line != "MEMORY") && (line != "")) {
					l=line.length();
					i=0;
					while (line[i] != 'R') i++; //find R, i point at R
					i++;
					head=i;
					while (line[i] !=' ') i++; //find number, i point at first space
					rear=i;
					reg_num=string2int(line.substr(head,rear-head));
					while (line[i] ==' ') i++; //ignore space, i point at first value
					head=i;
					while ((line[i] !=' ') && (i<l)) i++; //find register value i point at last
					rear=i;
					reg_value=string2int(line.substr(head,rear-head));
					Register[reg_num][0]=reg_value;
					getline(fin,line);
					if (line[line.length()-1]=='\r') line=line.substr(0,line.length()-1);
				}
			} else if (line=="MEMORY") {
				getline(fin,line);
				if (line[line.length()-1]=='\r') line=line.substr(0,line.length()-1);
				while ((line != "") && (line != "CODE")) {
					l=line.length();
					i=0;
					while (line[i] == ' ') i++; //find memory address; i point at first address
					head=i;
					while (line[i] != ' ') i++; //i point at first space
					rear=i;
					mem_add=string2int(line.substr(head,rear-head)) / 8;
					while (line[i] == ' ') i++;
					head=i;
					while ((line[i] != ' ') && (i<l)) i++;
					rear=i;
					mem_value=string2int(line.substr(head,rear-head));
					Memory[mem_add]=mem_value;
					getline(fin,line);
					if (line[line.length()-1]=='\r') line=line.substr(0,line.length()-1);
				}
			} else if (line=="CODE") { //Actually, decode stage has been done in this process.
				getline(fin,line);
				if (line[line.length()-1]=='\r') line=line.substr(0,line.length()-1);
				inst_add=0;
				num_label=0;
				while ((line != "")) { //FIXME condition may be wrong  && !fin.eof()
					l=line.length();
					i=0; //FIXME check i!!
					while (line[i] == ' ') i++; //point at first char
					head=i;
					while (((line[i] >= 'A') && (line[i] <= 'Z')) || ((line[i] >= 'a') && (line[i] <= 'z'))) i++; //indentify char, following is ':' or space
					rear=i;
					t=line.substr(head,rear-head);
					if (t=="DADD") { //DADD R1(rd), R2(rs), R3(rt)/#20/#-10(immediate)  R1=R2+R3 R-type,I-type
						type='R';
						type_rs=true;
						type_rt=true;
						rs=No_value;
						rd=No_value;
						rt=No_value;
						immediate=No_value;
						while (line[i] != 'R') i++;
						i++;
						head=i;
						while ((line[i] >= '0') && (line[i] <= '9')) i++; //i point at char right after number:, or space
						rear=i;
						rd=string2int(line.substr(head,rear-head));
						//process for rs
						while ((line[i] != 'R') && (line[i] != '#')) i++; //i point at R or #
						if (line[i] == '#') type_rs=false;
		 				i++;
						head=i;
						while (((line[i] >= '0') && (line[i] <= '9')) || (line[i] == '-')) i++;
						rear=i;
						if (type_rs) rs=string2int(line.substr(head,rear-head)); //R-type
						else immediate=string2int(line.substr(head,rear-head)); //I-type
						//process for rt
						while ((line[i] != 'R') && (line[i] != '#')) i++; //i point at R or #
						if (line[i] == '#') type_rt=false;
						i++;
						head=i;
						while ((((line[i] >= '0') && (line[i] <= '9')) || (line[i] == '-')) && (i<l)) i++;
						rear=i;
						if (type_rt) rt=string2int(line.substr(head,rear-head)); //R-type
						else immediate=string2int(line.substr(head,rear-head)); //I-type		
						if (type_rs && type_rt) type='R'; else type='I';
						
						Instruction[inst_add].op=t;
						Instruction[inst_add].type=type;
						Instruction[inst_add].rs=rs;
						Instruction[inst_add].rt=rt;
						Instruction[inst_add].rd=rd;
						Instruction[inst_add].immediate=immediate;
						
						inst_add++;
						getline(fin,line);
						if (line[line.length()-1]=='\r') line=line.substr(0,line.length()-1);
					} else if (t=="SUB") { //SUB R1, R2, R3/#20(immediate)  R1=R2-R3 Register,Immediate/R-type,I-type
						type='R';
						type_rs=true;
						type_rt=true;
						rs=No_value;
						rd=No_value;
						rt=No_value;
						immediate=No_value;
						while (line[i] != 'R') i++;
						i++;
						head=i;
						while ((line[i] >= '0') && (line[i] <= '9')) i++; //i point at char right after number:, or space
						rear=i;
						rd=string2int(line.substr(head,rear-head));
						//process for rs
						while ((line[i] != 'R') && (line[i] != '#')) i++; //i point at R or #
						if (line[i] == '#') type_rs=false;
						i++;
						head=i;
						while (((line[i] >= '0') && (line[i] <= '9')) || (line[i] == '-')) i++;
						rear=i;
						if (type_rs) rs=string2int(line.substr(head,rear-head)); //R-type
						else immediate=string2int(line.substr(head,rear-head)); //I-type
						//process for rt
						while ((line[i] != 'R') && (line[i] != '#')) i++; //i point at R or #
						if (line[i] == '#') type_rt=false;
						i++;
						head=i;
						while ((((line[i] >= '0') && (line[i] <= '9')) || (line[i] == '-')) && (i<l)) i++;
						rear=i;
						if (type_rt) rt=string2int(line.substr(head,rear-head)); //R-type
						else immediate=string2int(line.substr(head,rear-head)); //I-type
								
						if (type_rs && type_rt) type='R'; else type='I';
						Instruction[inst_add].op=t;
						Instruction[inst_add].type=type;
						Instruction[inst_add].rs=rs;
						Instruction[inst_add].rt=rt;
						Instruction[inst_add].rd=rd;
						Instruction[inst_add].immediate=immediate;
						
						inst_add++;
						getline(fin,line);
						if (line[line.length()-1]=='\r') line=line.substr(0,line.length()-1);
					} else if (t=="BNEZ") { //BNEZ R1(rt), LOOP  jump to LOOP if R1!=0 I-type
						type='I';
						rt=No_value;
						jump="";
						while (line[i] != 'R') i++;
						i++;
						head=i;
						while ((line[i] >= '0') && (line[i] <= '9')) i++; //i point at char right after number:, or space
						rear=i;
						rt=string2int(line.substr(head,rear-head));
								
						while (!((line[i] >= 'A') && (line[i] <= 'Z'))) i++;
						head=i;
						while ( (((line[i] >= 'A') && (line[i] <= 'Z')) || ((line[i] >= 'a') && (line[i] <= 'z'))) && (i<l)) i++;
						rear=i;
						jump=line.substr(head,rear-head);
						Instruction[inst_add].op=t;
						Instruction[inst_add].type=type;
						Instruction[inst_add].rt=rt;
						Instruction[inst_add].jump=jump;
								
						inst_add++;
						getline(fin,line);
						if (line[line.length()-1]=='\r') line=line.substr(0,line.length()-1);
					} else if (t=="LD") { //LD R1(rt), (R2)/8(R2)/-10(R2)(rs,immediate) Register_Indirect,Displacement/I-type
						type='I';
						rt=No_value;
						rs=No_value;
						immediate=No_value;
						while (line[i] != 'R') i++;
						i++;
						head=i;
						while ((line[i] >= '0') && (line[i] <= '9')) i++; //i point at char right after number:, or space
						rear=i;
						rt=string2int(line.substr(head,rear-head));
						
						while ( !(((line[i] >='0')&&(line[i]<='9')) || (line[i]=='-') || (line[i]=='(')) ) i++;
						if (line[i] == '(') immediate=0;
						else {
							head=i;
							while (line[i] != '(') i++;
							rear=i;
							immediate=string2int(line.substr(head,rear-head));
						}
						while (line[i]!='R') i++;
						i++;
						head=i;
						while ((line[i]>='0')&&(line[i]<='9')) i++;
						rear=i;
						rs=string2int(line.substr(head,rear-head));
								
						Instruction[inst_add].op=t;
						Instruction[inst_add].type=type;
						Instruction[inst_add].rt=rt;
						Instruction[inst_add].rs=rs;
						Instruction[inst_add].immediate=immediate / 8;
								
						inst_add++;
						getline(fin,line);
						if (line[line.length()-1]=='\r') line=line.substr(0,line.length()-1);
					} else if (t=="SD") { //SD R1, (R2)/8(R2)/-10(R2)
						type='I';
						rt=No_value;
						rs=No_value;
						immediate=No_value;
						while (line[i] != 'R') i++;
						i++;
						head=i;
						while ((line[i] >= '0') && (line[i] <= '9')) i++; //i point at char right after number:, or space
						rear=i;
						rt=string2int(line.substr(head,rear-head));
								
						while ( !(((line[i] >='0')&&(line[i]<='9')) || (line[i]=='-') || (line[i]=='(')) ) i++;
						if (line[i] == '(') immediate=0;
						else {
							head=i;
							while (line[i] != '(') i++;
							rear=i;
							immediate=string2int(line.substr(head,rear-head));
						}
						while (line[i]!='R') i++;
						i++;
						head=i;
						while ((line[i]>='0')&&(line[i]<='9')) i++;
						rear=i;
						rs=string2int(line.substr(head,rear-head));
								
						Instruction[inst_add].op=t;
						Instruction[inst_add].type=type;
						Instruction[inst_add].rt=rt;
						Instruction[inst_add].rs=rs;
						Instruction[inst_add].immediate=immediate / 8;
								
						inst_add++;
						getline(fin,line);
						if (line[line.length()-1]=='\r') line=line.substr(0,line.length()-1);
					} else { //label, we assume label and code are at the same line
						Label[num_label].name=t;
						Label[num_label].inst_add=inst_add;
						while (!((line[i] >= 'A') && (line[i] <= 'Z'))) i++;
						line=line.substr(i,l-i); //shrink line
						if (line[line.length()-1]=='\r') line=line.substr(0,line.length()-1);
						num_label++;
					}
				} // end while ()
			} else {  // if (line=="CODE")
				getline(fin,line);
				if (line[line.length()-1]=='\r') line=line.substr(0,line.length()-1);
			}
		fin.close();
	} else { // if (fin.is_open())
		cout<<"Unable to open the input file:"<<sinput<<endl;
		fin.close(); //FIXME I don't know whether to close or not.
		return false;
	}
	return true;	
}

void analyze_inst() { //change BNEZ jump label to immediate inst address
	int i,j;

	for (i=0;i<inst_add;i++)
		if (Instruction[i].op=="BNEZ")
			for (j=0;j<num_label;j++)
				if (Instruction[i].jump==Label[j].name) {
					Instruction[i].immediate=Label[j].inst_add;
					Instruction[i].jump="";
				}
}

void IF1() { //fetch inst, update PC
	if (inst_add!=0)
		if (Pipeline_reg[0].status != 2) {//if not stall
			if (PC_updated) { //PC updated begin to fetch
				inst_num++; //globle instruction num++
				Pipeline_reg[0].status=1; //status updated occupied;
				Pipeline_reg[0].Inst_num=inst_num;
				Pipeline_reg[0].Instruction=Instruction[PC]; //write Instuction
			}
			if (PC==inst_add-1) PC_updated=false; //update pc
			else {
				PC_updated=true;
				PC++;
			}
		}	
}

void IF2() { //FIXME check stall status
	if (Pipeline_reg[1].status == 2) //IF2 stall
		Pipeline_reg[0].status=2;
	else if ((Pipeline_reg[0].status!=0)&&(Pipeline_reg[1].status==0)) {
		Pipeline_reg[1]=Pipeline_reg[0];//move from previous stage to next
		Pipeline_reg[0].status=0; //update status
		Pipeline_reg[1].status=1;
	}
}

void ID() {
	if (Pipeline_reg[2].status == 2) //ID stall
		Pipeline_reg[1].status=2;
	else if ((Pipeline_reg[1].status!=0)&&(Pipeline_reg[2].status==0)) {
		Pipeline_reg[2]=Pipeline_reg[1];//move from previous stage to next
		Pipeline_reg[1].status=0; //update status
		Pipeline_reg[2].status=1;
	}
}

void EXE() {//FIXME handle all move statement;
	int i;
	int indicator;

	if (Pipeline_reg[3].status == 2) //EXE stall
		Pipeline_reg[2].status=2;
	else if ((Pipeline_reg[2].status!=0)&&(Pipeline_reg[3].status==0)) {
		if ((Pipeline_reg[2].Instruction.op=="DADD") || (Pipeline_reg[2].Instruction.op=="SUB")) {
			if (Pipeline_reg[2].Instruction.op=="DADD") indicator=1;
			else indicator=-1;

			if (Pipeline_reg[2].Instruction.type=='R') {
				if ((Register[Pipeline_reg[2].Instruction.rs][1]==0)&&(Register[Pipeline_reg[2].Instruction.rt][1]==0)) { 
				//no RAW, begin to execute
					//execute	
					Pipeline_reg[2].Reg_num=Pipeline_reg[2].Instruction.rd;
					Pipeline_reg[2].Reg_value=Register[Pipeline_reg[2].Instruction.rs][0]+Register[Pipeline_reg[2].Instruction.rt][0]*indicator;
					Pipeline_reg[2].Reg_cycle=cycle;
					//move
					Pipeline_reg[3]=Pipeline_reg[2];
					Pipeline_reg[2].status=0;
					Pipeline_reg[3].status=1;
					Register[Pipeline_reg[3].Instruction.rd][1]=Pipeline_reg[3].Inst_num;
				} else {
					if (Register[Pipeline_reg[2].Instruction.rs][1]==0) //0 available
						Pipeline_reg[2].rs_value=Register[Pipeline_reg[2].Instruction.rs][0];

					if (Register[Pipeline_reg[2].Instruction.rt][1]==0) //0 available
						Pipeline_reg[2].rt_value=Register[Pipeline_reg[2].Instruction.rt][0];
					//forwarding
					for(i=3;i<Pipeline_reg_num;i++) {
						if ((Pipeline_reg[i].status!=0)&&(Pipeline_reg[i].Reg_num==Pipeline_reg[2].Instruction.rs)&&(Pipeline_reg[2].rs_value==No_value)) {
						Pipeline_reg[2].rs_value=Pipeline_reg[i].Reg_value;
						Pipeline_reg[2].rs_cycle=Pipeline_reg[i].Reg_cycle;
						}
						if ((Pipeline_reg[i].status!=0)&&(Pipeline_reg[i].Reg_num==Pipeline_reg[2].Instruction.rt)&&(Pipeline_reg[2].rt_value==No_value)) {
						Pipeline_reg[2].rt_value=Pipeline_reg[i].Reg_value;
						Pipeline_reg[2].rt_cycle=Pipeline_reg[i].Reg_cycle;
						}
					}
					if ((Pipeline_reg[2].rt_value==No_value) || (Pipeline_reg[2].rs_value==No_value)) 
						Pipeline_reg[2].status=2;
					else if ((Pipeline_reg[2].rs_cycle<cycle) && (Pipeline_reg[2].rt_cycle<cycle)) {
						//execute	
						Pipeline_reg[2].Reg_num=Pipeline_reg[2].Instruction.rd;
						Pipeline_reg[2].Reg_value=Pipeline_reg[2].rs_value+Pipeline_reg[2].rt_value*indicator;
						Pipeline_reg[2].Reg_cycle=cycle;
						//move
						Pipeline_reg[3]=Pipeline_reg[2];
						Pipeline_reg[2].status=0;
						Pipeline_reg[3].status=1;
						Register[Pipeline_reg[3].Instruction.rd][1]=Pipeline_reg[3].Inst_num;
					} else Pipeline_reg[2].status=2;
				}
			} else if (Pipeline_reg[2].Instruction.type=='I') {
				if (Pipeline_reg[2].Instruction.rs==No_value) { //dadd rd immediate rt 
					if (Register[Pipeline_reg[2].Instruction.rt][1]==0) { 
					//no RAW, begin to execute
						//execute	
						Pipeline_reg[2].Reg_num=Pipeline_reg[2].Instruction.rd;
						Pipeline_reg[2].Reg_value=Pipeline_reg[2].Instruction.immediate+Register[Pipeline_reg[2].Instruction.rt][0]*indicator;
						Pipeline_reg[2].Reg_cycle=cycle;
						//move
						Pipeline_reg[3]=Pipeline_reg[2];
						Pipeline_reg[2].status=0;
						Pipeline_reg[3].status=1;
						Register[Pipeline_reg[3].Instruction.rd][1]=Pipeline_reg[3].Inst_num;
					} else {
						//forwarding
						for(i=3;i<Pipeline_reg_num;i++) {
							if ((Pipeline_reg[i].status!=0)&&(Pipeline_reg[i].Reg_num==Pipeline_reg[2].Instruction.rt)&&(Pipeline_reg[2].rt_value==No_value)) {
							Pipeline_reg[2].rt_value=Pipeline_reg[i].Reg_value;
							Pipeline_reg[2].rt_cycle=Pipeline_reg[i].Reg_cycle;
							}
						}
						if (Pipeline_reg[2].rt_value==No_value) 
							Pipeline_reg[2].status=2;
						else if (Pipeline_reg[2].rt_cycle<cycle) {
							//execute	
							Pipeline_reg[2].Reg_num=Pipeline_reg[2].Instruction.rd;
							Pipeline_reg[2].Reg_value=Pipeline_reg[2].Instruction.immediate+Pipeline_reg[2].rt_value*indicator;
							Pipeline_reg[2].Reg_cycle=cycle;
							//move
							Pipeline_reg[3]=Pipeline_reg[2];
							Pipeline_reg[2].status=0;
							Pipeline_reg[3].status=1;
							Register[Pipeline_reg[3].Instruction.rd][1]=Pipeline_reg[3].Inst_num;
						} else Pipeline_reg[2].status=2;
					}
							
				} else if (Pipeline_reg[2].Instruction.rt==No_value) { //dadd rd rs immediate
					if (Register[Pipeline_reg[2].Instruction.rs][1]==0) { 
					//no RAW, begin to execute
						//execute	
						Pipeline_reg[2].Reg_num=Pipeline_reg[2].Instruction.rd;
						Pipeline_reg[2].Reg_value=Register[Pipeline_reg[2].Instruction.rs][0]+Pipeline_reg[2].Instruction.immediate*indicator;
						Pipeline_reg[2].Reg_cycle=cycle;
						//move
						Pipeline_reg[3]=Pipeline_reg[2];
						Pipeline_reg[2].status=0;
						Pipeline_reg[3].status=1;
						Register[Pipeline_reg[3].Instruction.rd][1]=Pipeline_reg[3].Inst_num;
					} else {
						//forwarding
						for(i=3;i<Pipeline_reg_num;i++) {
							if ((Pipeline_reg[i].status!=0)&&(Pipeline_reg[i].Reg_num==Pipeline_reg[2].Instruction.rs)&&(Pipeline_reg[2].rs_value==No_value)) {
							Pipeline_reg[2].rs_value=Pipeline_reg[i].Reg_value;
							Pipeline_reg[2].rs_cycle=Pipeline_reg[i].Reg_cycle;
							}
						}
						if (Pipeline_reg[2].rs_value==No_value) 
							Pipeline_reg[2].status=2;
						else if (Pipeline_reg[2].rs_cycle<cycle) {
							//execute	
							Pipeline_reg[2].Reg_num=Pipeline_reg[2].Instruction.rd;
							Pipeline_reg[2].Reg_value=Pipeline_reg[2].rs_value+Pipeline_reg[2].Instruction.immediate*indicator;
							Pipeline_reg[2].Reg_cycle=cycle;
							//move
							Pipeline_reg[3]=Pipeline_reg[2];
							Pipeline_reg[2].status=0;
							Pipeline_reg[3].status=1;
							Register[Pipeline_reg[3].Instruction.rd][1]=Pipeline_reg[3].Inst_num;
						} else Pipeline_reg[2].status=2;
					}
				}
			}
		} else if (Pipeline_reg[2].Instruction.op=="LD") {//check rs to handle stall
			if (Register[Pipeline_reg[2].Instruction.rs][1]==0) { 
				//no RAW, begin to execute
				//execute
				Pipeline_reg[2].rs_value=Register[Pipeline_reg[2].Instruction.rs][0] / 8;
				//move
				Pipeline_reg[3]=Pipeline_reg[2];
				Pipeline_reg[2].status=0;
				Pipeline_reg[3].status=1;
				Register[Pipeline_reg[3].Instruction.rt][1]=Pipeline_reg[3].Inst_num;
			} else {
				//forwarding
				for(i=3;i<Pipeline_reg_num;i++) {
					if ((Pipeline_reg[i].status!=0)&&(Pipeline_reg[i].Reg_num==Pipeline_reg[2].Instruction.rs)&&(Pipeline_reg[2].rs_value==No_value)) {
					Pipeline_reg[2].rs_value=Pipeline_reg[i].Reg_value / 8;
					Pipeline_reg[2].rs_cycle=Pipeline_reg[i].Reg_cycle;
					}
				}
				if (Pipeline_reg[2].rs_value==No_value)
					Pipeline_reg[2].status=2;
				else if (Pipeline_reg[2].rs_cycle<cycle) {
					//move
					Pipeline_reg[3]=Pipeline_reg[2];
					Pipeline_reg[2].status=0;
					Pipeline_reg[3].status=1;
					Register[Pipeline_reg[3].Instruction.rt][1]=Pipeline_reg[3].Inst_num;
				} else Pipeline_reg[2].status=2;
			}
		} else if (Pipeline_reg[2].Instruction.op=="SD") {//check rs,rt to handel stall
			if ((Register[Pipeline_reg[2].Instruction.rs][1]==0)&&(Register[Pipeline_reg[2].Instruction.rt][1]==0)) { 
				//no RAW, begin to execute
				//execute
				Pipeline_reg[2].rs_value=Register[Pipeline_reg[2].Instruction.rs][0] / 8;
				Pipeline_reg[2].rt_value=Register[Pipeline_reg[2].Instruction.rt][0];
				//move
				Pipeline_reg[3]=Pipeline_reg[2];
				Pipeline_reg[2].status=0;
				Pipeline_reg[3].status=1;
			} else {
				if (Register[Pipeline_reg[2].Instruction.rs][1]==0) //0 available
					Pipeline_reg[2].rs_value=Register[Pipeline_reg[2].Instruction.rs][0] / 8;
				if (Register[Pipeline_reg[2].Instruction.rt][1]==0) //0 available
					Pipeline_reg[2].rt_value=Register[Pipeline_reg[2].Instruction.rt][0];
				//forwarding
				for(i=3;i<Pipeline_reg_num;i++) {
					if ((Pipeline_reg[i].status!=0)&&(Pipeline_reg[i].Reg_num==Pipeline_reg[2].Instruction.rs)&&(Pipeline_reg[2].rs_value==No_value)) {
					Pipeline_reg[2].rs_value=Pipeline_reg[i].Reg_value / 8;
					Pipeline_reg[2].rs_cycle=Pipeline_reg[i].Reg_cycle;
					}
					if ((Pipeline_reg[i].status!=0)&&(Pipeline_reg[i].Reg_num==Pipeline_reg[2].Instruction.rt)&&(Pipeline_reg[2].rt_value==No_value)) {
					Pipeline_reg[2].rt_value=Pipeline_reg[i].Reg_value;
					Pipeline_reg[2].rt_cycle=Pipeline_reg[i].Reg_cycle;
					}
				}
				if ((Pipeline_reg[2].rt_value==No_value) || (Pipeline_reg[2].rs_value==No_value)) 
					Pipeline_reg[2].status=2;
				else if ((Pipeline_reg[2].rs_cycle<cycle) && (Pipeline_reg[2].rt_cycle<cycle)) {
					//move
					Pipeline_reg[3]=Pipeline_reg[2];
					Pipeline_reg[2].status=0;
					Pipeline_reg[3].status=1;
				} else Pipeline_reg[2].status=2;
			}

		} else if (Pipeline_reg[2].Instruction.op=="BNEZ") {
			if (Register[Pipeline_reg[2].Instruction.rt][1]==0) { 
				//no RAW, begin to execute
				//execute
				Pipeline_reg[2].rt_value=Register[Pipeline_reg[2].Instruction.rt][0];
				
				if (Pipeline_reg[2].rt_value==0) { //prediction taken
					is_flushed=true;
					PC_target=Pipeline_reg[2].Instruction.immediate;
				}
				//move
				Pipeline_reg[3]=Pipeline_reg[2];
				Pipeline_reg[2].status=0;
				Pipeline_reg[3].status=1;
			} else {
				//forwarding
				for(i=3;i<Pipeline_reg_num;i++) {
					if ((Pipeline_reg[i].status!=0)&&(Pipeline_reg[i].Reg_num==Pipeline_reg[2].Instruction.rt)&&(Pipeline_reg[2].rt_value==No_value)) {
					Pipeline_reg[2].rt_value=Pipeline_reg[i].Reg_value;
					Pipeline_reg[2].rt_cycle=Pipeline_reg[i].Reg_cycle;
					}
				}
				if (Pipeline_reg[2].rt_value==No_value)
					Pipeline_reg[2].status=2;
				else if (Pipeline_reg[2].rt_cycle<cycle) {
					//execute
					if (Pipeline_reg[2].rt_value!=0) { //prediction taken
						is_flushed=true;
						PC_target=Pipeline_reg[2].Instruction.immediate;
					} 
					//move
					Pipeline_reg[3]=Pipeline_reg[2];
					Pipeline_reg[2].status=0;
					Pipeline_reg[3].status=1;
				} else Pipeline_reg[2].status=2;
			}
		}
	}
}

void MEM1() { //handle flush
	int i;
	
	if (Pipeline_reg[4].status == 2) //MEM1 stall
		Pipeline_reg[3].status=2;
	else if ((Pipeline_reg[3].status!=0)&&(Pipeline_reg[4].status==0)) {
		Pipeline_reg[4]=Pipeline_reg[3];//move from previous stage to next
		Pipeline_reg[3].status=0; //update status
		Pipeline_reg[4].status=1;

		if (is_flushed) {
			for (i=0;i<3;i++) 
				Pipeline_reg[i].status=0;
			is_flushed=false;
			PC=PC_target;
			PC_updated=true;
		}	
	}
}

void MEM2() { //handle sd ld output
	if (Pipeline_reg[5].status == 2) //MEM2 stall
		Pipeline_reg[4].status=2;
	else if ((Pipeline_reg[4].status!=0)&&(Pipeline_reg[5].status==0)) {
		if (Pipeline_reg[4].Instruction.op=="LD"){
			Pipeline_reg[4].Reg_value=Memory[Pipeline_reg[4].rs_value+Pipeline_reg[4].Instruction.immediate];
			Pipeline_reg[4].Reg_num=Pipeline_reg[4].Instruction.rt;
			Pipeline_reg[4].Reg_cycle=cycle;
		} else if (Pipeline_reg[4].Instruction.op=="SD")
			Memory[Pipeline_reg[4].rs_value+Pipeline_reg[4].Instruction.immediate]=Pipeline_reg[4].rt_value;
		//move
		Pipeline_reg[5]=Pipeline_reg[4];//move from previous stage to next
		Pipeline_reg[4].status=0; //update status
		Pipeline_reg[5].status=1;
	}
}

void MEM3() {
	if (Pipeline_reg[6].status == 2) //MEM3 stall
		Pipeline_reg[5].status=2;
	else if ((Pipeline_reg[5].status!=0)&&(Pipeline_reg[6].status==0)) {
		Pipeline_reg[6]=Pipeline_reg[5];//move from previous stage to next
		Pipeline_reg[5].status=0; //update status
		Pipeline_reg[6].status=1;
	}	
}

void WB() {
	if (Pipeline_reg[7].status == 2) //MEM3 stall
		Pipeline_reg[6].status=2;
	else if ((Pipeline_reg[6].status!=0)) {
		Pipeline_reg[7]=Pipeline_reg[6];//move from previous stage to next
		Pipeline_reg[6].status=0; //update status
		Pipeline_reg[7].status=1;
		//write register
		if ((Pipeline_reg[7].Instruction.op=="LD") || (Pipeline_reg[7].Instruction.op=="DADD") || (Pipeline_reg[7].Instruction.op=="SUB")) {
			if (Register[Pipeline_reg[7].Reg_num][1]=Pipeline_reg[7].Inst_num) { //avoid early releasing
				Register[Pipeline_reg[7].Reg_num][0]=Pipeline_reg[7].Reg_value;
				Register[Pipeline_reg[7].Reg_num][1]=0; //available
			}
		} 
	} else if ((Pipeline_reg[6].status==0)&&(Pipeline_reg[7].status!=0))
		Pipeline_reg[7].status=0;

}

void write_pipeline_res() {
	int i;
	string stage[8];

	stage[0]="IF1";
	stage[1]="IF2";
	stage[2]="ID";
	stage[3]="EX";
	stage[4]="MEM1";
	stage[5]="MEM2";
	stage[6]="MEM3";
	stage[7]="WB";

	fout<<"c#"<<cycle<<"  ";
	for(i=Pipeline_reg_num-1;i>=0;i--) {
		if (Pipeline_reg[i].status != 0)
			if (Pipeline_reg[i].status == 1)
				fout<<"I"<<Pipeline_reg[i].Inst_num<<"-"<<stage[i]<<" ";
			else if (Pipeline_reg[i].status == 2)
				fout<<"I"<<Pipeline_reg[i].Inst_num<<"-"<<"stall"<<" ";
	}
	fout<<endl;
}

void write_static_res() { //output register and memory value
	int i;
	
	fout<<"REGISTER"<<endl;
	for(i=0;i<Max_reg;i++)
		if (Register[i][0]!=0) //value !=0 then output
			fout<<"R"<<i<<" "<<Register[i][0]<<endl;
			
	fout<<"MEMORY"<<endl;
	for(i=0;i<Max_mem;i++)
		if (Memory[i]!=0)
			fout<<i*8<<" "<<Memory[i]<<endl;
}

void tick() {
	WB();
	MEM3();
	MEM2();
	MEM1();
	EXE();
	ID();
	IF2();
	IF1();	
	write_pipeline_res();
}

bool is_pipleline_completed() {
	int i;
	bool res;
	res=false;
	if (!PC_updated)
		for(i=0;i<Pipeline_reg_num-1;i++)
			if (Pipeline_reg[i].status==0) res=true;
			else {
				res=false;
				return res;
			}
	return res;
}

void init_global() {
	is_flushed=false;
	PC=0;
	PC_target=No_value;
	PC_updated=true;
	cycle=0;
	inst_num=0;
}

int main() {
	string sinput; //users input
	init(); //initialize
	
	cout<<"Please type the name of input file followed by an enter. If you want to quit, input 'q':";
	cin>>sinput;	
	while (sinput != "q") {
		init(); //initialized
		if (analyze_src(sinput)) {
			analyze_inst();
			cout<<"Start to simulate."<<endl;
			//intialized global variables
			init_global();
			//prepare for ouput
			sinput=sinput+".out";
			fout.open(sinput.c_str());
			//start to simulate
			do {
				cycle++;
				tick();
			} while (!is_pipleline_completed());
			//output results
			write_static_res();
			fout.close();
			cout<<"Results have been written into "<<sinput<<endl;

			cout<<"Please type the name of input file followed by an enter. If you want to quit, type 'q':";
			cin>>sinput;
		} else {
			cout<<"Please type the name of input file followed by an enter. If you want to quit, type 'q':";
			cin>>sinput;
		}
	}
	cout<<"Simulation finished."<<endl;
	return 0;
}
