#include "ros/ros.h"
#include "std_msgs/String.h"

#include <sstream>

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

#include <iostream>
#include <vector>
#include <algorithm>
#include <numeric>

#include <can_test_02/recv.h>
#include <can_test_02/orig.h>
#include <can_test_02/orig_new.h>
#include <can_test_02/UpToDown.h>
#include <can_test_02/DownToUp.h>

#include "lib_can/controlcan.h"/* 包含CAN通讯硬件操作函数头文件 */

#include "dlfcn.h" /* 包含动态链接功能接口文件 */

#define SOFILE "/home/null/ros_ws/modular_joint_ws_1/libcontrolcan.so" /* 指定动态链接库名称 */
#define PI 3.14159265359
/**
 * This tutorial demonstrates simple sending of messages over the ROS system.
 */

using namespace std;

int my_test = 0;

//-------------函数声明--------------
int send_command();
int send_command_position();
int send_command_speed();
int send_command_current();
int send_command_position_PID();
int send_command_speed_direct();
int send_command_stop();
int send_command_zero();
int send_command_sdo(int ID, int IDX, int DATA);
//-------------函数声明--------------

//-------------校验位声明--------------
/*
int control_front[1] = { 0x69 };//控制帧前校验位
//int control_back[1] = { 0x33 };//控制帧后校验位

int position_speed_front[1] = { 0x63 };//运动状态电流帧前校验位
int position_speed_back[1] = { 0x40 };//运动状态帧后校验位

int temperature_current_front[3] = { 0x33,0x22,0x11 };//温度帧前校验位
int temperature_current_back[1] = { 0x02 };//监视帧后校验位

int acceleration_front[1] = { 0x32 };//加速度帧前校验位
int acceleration_back[1] = { 0x11 };//加速度帧后校验位

int angular_speed_front[1] = { 0x73 };//角速度帧前校验位
int angular_speed_back[1] = { 0x82 };//角速度帧后校验位

int rpy_angle_front[1] = { 0x93 };//姿态角帧前校验位
int rpy_angle_back[1] = { 0x29 };//姿态角帧后校验位

int joint_type_front[3] = { 0x37,0x93,0x28 };//关节硬件类型帧前校验位
int joint_type_back[1] = { 0x42 };//关节硬件类型帧后校验位

int joint_ready_front[3] = { 0x72,0x24,0x35 };//关节准备状态帧前校验位
int joint_ready_back[1] = { 0x19 };//关节准备状态帧后校验位
*/
//-------------校验位声明--------------


struct CommandFrame
{
	vector<int64_t>ID;
	vector<double>POSITION;
	vector<double>SPEED;
	vector<double>CURRENT;
	//double TIME;
	//vector<double>TIMES;
};

struct CanCommandFrame
{
	UINT ID;
	BYTE SendType;
	BYTE RemoteFlag;
	BYTE ExternFlag;
	BYTE DataLen;
	BYTE Data[8];
};

vector <CommandFrame> command_received;
vector <CanCommandFrame> command_to_send;
vector <double> current_times;
vector <double> times_received;

/*
bool SortByTime(CommandFrame f1, CommandFrame f2)
{
	return (f1.TIME < f2.TIME);
}
*/


//-----------------接收命令并处理---------------------
class CommandReceiver
{
public:
	CommandReceiver()
	{
		//Topic you want to publish
		//chatter_pub_1 = n.advertise<can_test_02::orig_new>("Comm_Orig", 1000);
		chatter_pub_0 = n.advertise<can_test_02::DownToUp>("down_to_up", 1000);
		//Topic you want to subscribe  
		sub = n.subscribe("up_to_down", 1000, &CommandReceiver::callback, this);
	}

	void callback(const can_test_02::UpToDown& MsgUpToDown)
	{
		int _mode = MsgUpToDown.MODE;

		if (_mode == 0)//急停
		{
			

			int result = send_command_stop();

			can_test_02::DownToUp MsgDownToUp;
			//MsgDownToUp.ID = -1;
			if (result == 0)
			{
				MsgDownToUp.TYPE = "STOP_SUCCEED";
			}
			else
			{
				MsgDownToUp.TYPE = "STOP_FALIED";
			}
			//MsgDownToUp.DATA = ;
			chatter_pub_0.publish(MsgDownToUp);//发送确MSG
			command_received.clear();
			return;
		}
		else if (_mode == 1)//位置控制
		{
			
			//检查变量范围
			for (int i = 0; i < MsgUpToDown.ID.size(); i++)
			{
				if (MsgUpToDown.ID[i] < 1 || MsgUpToDown.ID[i]>255)//1~255
				{
					printf("\nerror: ID out of range.\n");
					return;
				}
			}
			for (int i = 0; i < MsgUpToDown.POSITION.size(); i++)
			{
				if (MsgUpToDown.POSITION[i] < -2*PI || MsgUpToDown.POSITION[i]>2*PI)//0~360
				{
					printf("\nerror: positon out of range.\n");
					return;
				}
			}
			for (int i = 0; i < MsgUpToDown.SPEED.size(); i++)
			{
				if (MsgUpToDown.SPEED[i] < -3.1416 || MsgUpToDown.SPEED[i]>3.1416)//-270~270
				{
					printf("\nerror: speed out of range.\n");
					return;
				}
			}
			for (int i = 0; i < MsgUpToDown.CURRENT.size(); i++)
			{
				if (MsgUpToDown.CURRENT[i] < -999 || MsgUpToDown.CURRENT[i]>999)//0~999
				{
					printf("\nerror: current out of range.\n");
					return;
				}
			}

			//ROS_INFO("command_recvd %d\n",(int)command_received.size());

			command_received.clear();		   //加入当前命令
			CommandFrame temp;
			temp.ID = MsgUpToDown.ID;
			temp.POSITION =MsgUpToDown.POSITION;
			temp.SPEED = MsgUpToDown.SPEED;
			temp.CURRENT = MsgUpToDown.CURRENT;
			//temp.TIME = MsgUpToDown.TIME;
			//temp.TIMES = MsgUpToDown.TIMES;
			command_received.push_back(temp);

			int result = send_command_position();

			can_test_02::DownToUp MsgDownToUp;
			//MsgDownToUp.ID = -1;
			if (result == 0)
			{
				MsgDownToUp.TYPE = "POSITION_CONTROL_SUCCEED";
			}
			else
			{
				MsgDownToUp.TYPE = "POSITION_CONTROL_FALIED";
			}
			//MsgDownToUp.DATA = ;
			chatter_pub_0.publish(MsgDownToUp);//发送确MSG
			command_received.clear();
			return;
		}
		else if (_mode == 2)//速度控制
		{
			
			//检查变量范围		
			for (int i = 0; i < MsgUpToDown.ID.size(); i++)
			{
				if (MsgUpToDown.ID[i] < 1 || MsgUpToDown.ID[i]>255)//1~255
				{
					printf("\nerror: ID out of range. ID %d\n",(int) MsgUpToDown.ID[i]);
					return;
				}
			}
			for (int i = 0; i < MsgUpToDown.SPEED.size(); i++)
			{
				if (MsgUpToDown.SPEED[i] < -270 || MsgUpToDown.SPEED[i]>270)//-270~270
				{
					printf("\nerror: speed out of range.\n");
					return;
				}
			}
			for (int i = 0; i < MsgUpToDown.CURRENT.size(); i++)
			{
				if (MsgUpToDown.CURRENT[i] < -999 || MsgUpToDown.CURRENT[i]>999)//0~999
				{
					printf("\nerror: current out of range.\n");
					return;
				}
			}
			//ROS_INFO("command_recvd %d\n",(int)command_received.size());

			command_received.clear();					 //清空命令缓冲区
			CommandFrame temp;							 //加入当前命令
			temp.ID = MsgUpToDown.ID;
			//temp.POSITION = MsgUpToDown.POSITION;
			temp.SPEED = MsgUpToDown.SPEED;
			//temp.CURRET = MsgUpToDown.CURRENT;
			//temp.TIME = MsgUpToDown.TIME;
			//temp.TIMES = MsgUpToDown.TIMES;
			command_received.push_back(temp);

			int result = send_command_speed();

			can_test_02::DownToUp MsgDownToUp;
			//MsgDownToUp.ID = -1;
			if (result == 0)
			{
				MsgDownToUp.TYPE = "SPEED_CONTROL_SUCCEED";
			}
			else
			{
				MsgDownToUp.TYPE = "SPEED_CONTROL_FALIED";
			}
			//MsgDownToUp.DATA = ;
			chatter_pub_0.publish(MsgDownToUp);//发送确MSG
			command_received.clear();
			return;

		}
		else if (_mode == 3)//控制ID指定
		{
			
			ROS_INFO("id receive %d,%d",(int)MsgUpToDown.ID[0],(int)MsgUpToDown.ID[1]);
			if (MsgUpToDown.ID.size() != 2)
			{
				printf("\nerror: ID quantity error.  %d\n",(int)MsgUpToDown.ID.size());
				
				return;
			}
			for (int i = 0; i < MsgUpToDown.ID.size(); i++)
			{
				printf("\nID   %d,%d,%d\n",(int)MsgUpToDown.ID.size(),(int)MsgUpToDown.ID[0],(int)MsgUpToDown.ID[1]);
				
				if (MsgUpToDown.ID[i] < 1 || MsgUpToDown.ID[i]>255)//1~255
				{
					printf("\nerror: ID out of range.\n");
					return;
				}
			}



			int result = send_command_sdo(MsgUpToDown.ID[0], 0, MsgUpToDown.ID[1]);

			can_test_02::DownToUp MsgDownToUp;
			//MsgDownToUp.ID = -1;
			if (result == 0)
			{
				MsgDownToUp.TYPE = "ID_REDEFINE_SUCCEED";
			}
			else
			{
				MsgDownToUp.TYPE = "ID_REDEFINE_FALIED";
			}
			//MsgDownToUp.DATA = ;
			chatter_pub_0.publish(MsgDownToUp);//发送确MSG
			command_received.clear();
			return;
		}
		else if (_mode == 4)//零位校准
		{
			command_received.clear();					 //清空命令缓冲区
			CommandFrame temp;							 //加入当前命令
			temp.ID = MsgUpToDown.ID;
			//temp.POSITION = MsgUpToDown.POSITION;
			//temp.SPEED = MsgUpToDown.SPEED;
			//temp.CURRET = MsgUpToDown.CURRENT;
			//temp.TIME = MsgUpToDown.TIME;
			//temp.TIMES = MsgUpToDown.TIMES;
			command_received.push_back(temp);
			int result = send_command_zero();
			can_test_02::DownToUp MsgDownToUp;
			//MsgDownToUp.ID = -1;
			if (result == 0)
			{
				MsgDownToUp.TYPE = "ID_REDEFINE_SUCCEED";
			}
			else
			{
				MsgDownToUp.TYPE = "ID_REDEFINE_FALIED";
			}
			//MsgDownToUp.DATA = ;
			chatter_pub_0.publish(MsgDownToUp);//发送确MSG
			command_received.clear();
			return;
		}
		else if (_mode == 5)//位置PID调试
		{
			
			if (MsgUpToDown.ID.size() != 1)
			{
				printf("\nerror: ID quantity error.\n");
				return;
			}
			if (MsgUpToDown.POSITION.size() != 3)
			{
				printf("\nerror: PID quantity error.\n");
				return;
			}
			for (int i = 0; i < MsgUpToDown.ID.size(); i++)
			{
				if (MsgUpToDown.ID[i] < 1 || MsgUpToDown.ID[i]>255)//1~255
				{
					printf("\nerror: ID out of range.\n");
					return;
				}
			}
			for (int i = 0; i < MsgUpToDown.POSITION.size(); i++)
			{
				if (MsgUpToDown.POSITION[i] < 0 || MsgUpToDown.POSITION[i]>99999)//1~99999
				{
					printf("\nerror: PID out of range.\n");
					return;
				}
			}

			int result1 = send_command_sdo(MsgUpToDown.ID[0], 11, (int)(MsgUpToDown.POSITION[0]*10));
			int result2 = send_command_sdo(MsgUpToDown.ID[0], 12, (int)(MsgUpToDown.POSITION[1]*10));
			int result3 = send_command_sdo(MsgUpToDown.ID[0], 13, (int)(MsgUpToDown.POSITION[2]*10));
			int result=result1 && result2 && result3;
			can_test_02::DownToUp MsgDownToUp;
			//MsgDownToUp.ID = -1;
			if (result == 0)
			{
				MsgDownToUp.TYPE = "PID_REDEFINE_SUCCEED";
			}
			else
			{
				MsgDownToUp.TYPE = "PID_REDEFINE_FALIED";
			}
			//MsgDownToUp.DATA = ;
			chatter_pub_0.publish(MsgDownToUp);//发送确MSG
			command_received.clear();
			return;
		}
		else if (_mode == 6)//速度PID调试
		{
			
			if (MsgUpToDown.ID.size() != 1)
			{
				printf("\nerror: ID quantity error.\n");
				return;
			}
			if (MsgUpToDown.POSITION.size() != 3)
			{
				printf("\nerror: PID quantity error.\n");
				return;
			}
			for (int i = 0; i < MsgUpToDown.ID.size(); i++)
			{
				if (MsgUpToDown.ID[i] < 1 || MsgUpToDown.ID[i]>255)//1~255
				{
					printf("\nerror: ID out of range.\n");
					return;
				}
			}
			for (int i = 0; i < MsgUpToDown.POSITION.size(); i++)
			{
				if (MsgUpToDown.POSITION[i] < 0 || MsgUpToDown.POSITION[i]>99999)//1~99999
				{
					printf("\nerror: PID out of range.\n");
					return;
				}
			}

			int result1 = send_command_sdo(MsgUpToDown.ID[0], 5, (int)(MsgUpToDown.POSITION[0]*10000));
			int result2 = send_command_sdo(MsgUpToDown.ID[0], 6, (int)(MsgUpToDown.POSITION[1]*10000));
			int result3 = send_command_sdo(MsgUpToDown.ID[0], 7, (int)(MsgUpToDown.POSITION[2]*10000));
			int result=result1 && result2 && result3;
			can_test_02::DownToUp MsgDownToUp;
			//MsgDownToUp.ID = -1;
			if (result == 0)
			{
				MsgDownToUp.TYPE = "PID_REDEFINE_SUCCEED";
			}
			else
			{
				MsgDownToUp.TYPE = "PID_REDEFINE_FALIED";
			}
			//MsgDownToUp.DATA = ;
			chatter_pub_0.publish(MsgDownToUp);//发送确MSG
			command_received.clear();
			return;
		}
		else if (_mode == 7)//位置PID写入
		{
			
			if (MsgUpToDown.ID.size() != 1)
			{
				printf("\nerror: ID quantity error.\n");
				return;
			}
			if (MsgUpToDown.POSITION.size() != 3)
			{
				printf("\nerror: PID quantity error.\n");
				return;
			}
			for (int i = 0; i < MsgUpToDown.ID.size(); i++)
			{
				if (MsgUpToDown.ID[i] < 1 || MsgUpToDown.ID[i]>255)//1~255
				{
					printf("\nerror: ID out of range.\n");
					return;
				}
			}
			for (int i = 0; i < MsgUpToDown.POSITION.size(); i++)
			{
				if (MsgUpToDown.POSITION[i] < 0 || MsgUpToDown.POSITION[i]>99999)//1~99999
				{
					printf("\nerror: PID out of range.\n");
					return;
				}
			}

			int result1 = send_command_sdo(MsgUpToDown.ID[0], 8, (int)(MsgUpToDown.POSITION[0]*10));
			int result2 = send_command_sdo(MsgUpToDown.ID[0], 9, (int)(MsgUpToDown.POSITION[1]*10));
			int result3 = send_command_sdo(MsgUpToDown.ID[0], 10, (int)(MsgUpToDown.POSITION[2]*10));
			int result=result1 && result2 && result3;
			can_test_02::DownToUp MsgDownToUp;
			//MsgDownToUp.ID = -1;
			if (result == 0)
			{
				MsgDownToUp.TYPE = "PID_REDEFINE_SUCCEED";
			}
			else
			{
				MsgDownToUp.TYPE = "PID_REDEFINE_FALIED";
			}
			//MsgDownToUp.DATA = ;
			chatter_pub_0.publish(MsgDownToUp);//发送确MSG
			command_received.clear();
			return;
		}
		else if (_mode == 8)//速度PID写入
		{
			
			if (MsgUpToDown.ID.size() != 1)
			{
				printf("\nerror: ID quantity error.\n");
				return;
			}
			if (MsgUpToDown.POSITION.size() != 3)
			{
				printf("\nerror: PID quantity error.\n");
				return;
			}
			for (int i = 0; i < MsgUpToDown.ID.size(); i++)
			{
				if (MsgUpToDown.ID[i] < 1 || MsgUpToDown.ID[i]>255)//1~255
				{
					printf("\nerror: ID out of range.\n");
					return;
				}
			}
			for (int i = 0; i < MsgUpToDown.POSITION.size(); i++)
			{
				if (MsgUpToDown.POSITION[i] < 0 || MsgUpToDown.POSITION[i]>99999)//1~99999
				{
					printf("\nerror: PID out of range.\n");
					return;
				}
			}

			int result1 = send_command_sdo(MsgUpToDown.ID[0], 2, (int)(MsgUpToDown.POSITION[0]*10000));
			int result2 =  send_command_sdo(MsgUpToDown.ID[0], 3, (int)(MsgUpToDown.POSITION[1]*10000));
			int result3 =  send_command_sdo(MsgUpToDown.ID[0], 4, (int)(MsgUpToDown.POSITION[2]*10000));
			int result=result1 && result2 && result3;


			can_test_02::DownToUp MsgDownToUp;
			//MsgDownToUp.ID = -1;
			if (result == 0)
			{
				MsgDownToUp.TYPE = "PID_REDEFINE_SUCCEED";
			}
			else
			{
				MsgDownToUp.TYPE = "PID_REDEFINE_FALIED";
			}
			//MsgDownToUp.DATA = ;
			chatter_pub_0.publish(MsgDownToUp);//发送确MSG
			command_received.clear();
			return;
		}
		else if (_mode == 9)//速度直接控制
		{
			
			//检查变量范围		
			for (int i = 0; i < MsgUpToDown.ID.size(); i++)
			{
				if (MsgUpToDown.ID[i] < 1 || MsgUpToDown.ID[i]>255)//1~255
				{
					printf("\nerror: ID out of range.\n");
					return;
				}
			}
			for (int i = 0; i < MsgUpToDown.SPEED.size(); i++)
			{
				if (MsgUpToDown.SPEED[i] < -270 || MsgUpToDown.SPEED[i]>270)//-270~270
				{
					printf("\nerror: speed out of range.\n");
					return;
				}
			}
			for (int i = 0; i < MsgUpToDown.CURRENT.size(); i++)
			{
				if (MsgUpToDown.CURRENT[i] < -999 || MsgUpToDown.CURRENT[i]>999)//0~999
				{
					printf("\nerror: current out of range.\n");
					return;
				}
			}
			//ROS_INFO("command_recvd %d\n",(int)command_received.size());

			command_received.clear();					 //清空命令缓冲区
			CommandFrame temp;							 //加入当前命令
			temp.ID = MsgUpToDown.ID;
			//temp.POSITION = MsgUpToDown.POSITION;
			temp.SPEED = MsgUpToDown.SPEED;
			//temp.CURRET = MsgUpToDown.CURRENT;
			//temp.TIME = MsgUpToDown.TIME;
			//temp.TIMES = MsgUpToDown.TIMES;
			command_received.push_back(temp);

			int result = send_command_speed_direct();

			can_test_02::DownToUp MsgDownToUp;
			//MsgDownToUp.ID = -1;
			if (result == 0)
			{
				MsgDownToUp.TYPE = "SPEED_CONTROL_SUCCEED";
			}
			else
			{
				MsgDownToUp.TYPE = "SPEED_CONTROL_FALIED";
			}
			//MsgDownToUp.DATA = ;
			chatter_pub_0.publish(MsgDownToUp);//发送确MSG
			command_received.clear();
			return;

		}
		else if (_mode == 10)//位置PID曲线运行
		{
			
			//检查变量范围		
			command_received.clear();					 //清空命令缓冲区
			CommandFrame temp;							 //加入当前命令
			temp.ID = MsgUpToDown.ID;
			//temp.POSITION = MsgUpToDown.POSITION;
			temp.SPEED = MsgUpToDown.SPEED;
			//temp.CURRET = MsgUpToDown.CURRENT;
			//temp.TIME = MsgUpToDown.TIME;
			//temp.TIMES = MsgUpToDown.TIMES;
			command_received.push_back(temp);
			int result = send_command_position_PID();

			can_test_02::DownToUp MsgDownToUp;
			//MsgDownToUp.ID = -1;
			if (result == 0)
			{
				MsgDownToUp.TYPE = "POSITION_PID_CONTROL_SUCCEED";
			}
			else
			{
				MsgDownToUp.TYPE = "POSITION_PID_CONTROL_FALIED";
			}
			//MsgDownToUp.DATA = ;
			chatter_pub_0.publish(MsgDownToUp);//发送确MSG
			command_received.clear();
			return;

		}
		else if (_mode == 11)//电流控制
		{
			
			//检查变量范围		
			for (int i = 0; i < MsgUpToDown.ID.size(); i++)
			{
				if (MsgUpToDown.ID[i] < 1 || MsgUpToDown.ID[i]>255)//1~255
				{
					printf("\nerror: ID out of range. ID %d\n",(int) MsgUpToDown.ID[i]);
					return;
				}
			}
			for (int i = 0; i < MsgUpToDown.SPEED.size(); i++)
			{
				if (MsgUpToDown.SPEED[i] < -270 || MsgUpToDown.SPEED[i]>270)//-270~270
				{
					printf("\nerror: speed out of range.\n");
					return;
				}
			}
			for (int i = 0; i < MsgUpToDown.CURRENT.size(); i++)
			{
				if (MsgUpToDown.CURRENT[i] < -999 || MsgUpToDown.CURRENT[i]>999)//0~999
				{
					printf("\nerror: current out of range.\n");
					return;
				}
			}
			//ROS_INFO("command_recvd %d\n",(int)command_received.size());

			command_received.clear();					 //清空命令缓冲区
			CommandFrame temp;							 //加入当前命令
			temp.ID = MsgUpToDown.ID;
			//temp.POSITION = MsgUpToDown.POSITION;
			//temp.SPEED = MsgUpToDown.SPEED;
			temp.CURRENT = MsgUpToDown.CURRENT;
			//temp.TIME = MsgUpToDown.TIME;
			//temp.TIMES = MsgUpToDown.TIMES;
			command_received.push_back(temp);

			int result = send_command_current();

			can_test_02::DownToUp MsgDownToUp;
			//MsgDownToUp.ID = -1;
			if (result == 0)
			{
				MsgDownToUp.TYPE = "CURRENT_CONTROL_SUCCEED";
			}
			else
			{
				MsgDownToUp.TYPE = "CURRENT_CONTROL_FALIED";
			}
			//MsgDownToUp.DATA = ;
			chatter_pub_0.publish(MsgDownToUp);//发送确MSG
			command_received.clear();
			return;

		}
		return;
		/*if (MsgUpToDown.TIMES.size() != 0)//普通模式
		{
			if (MsgUpToDown.ID.size() == MsgUpToDown.POSITION.size() == MsgUpToDown.SPEED.size())
			{
				vector<double>TIMES = MsgUpToDown.TIMES;
				double time = MsgUpToDown.TIME;
				//vector<double> TIMES(MsgUpToDown.TIMES, MsgUpToDown.TIMES+sizeof(MsgUpToDown.TIMES));
				sort(TIMES.begin(), TIMES.end());
				sort(current_times.begin(), current_times.end());
				sort(times_received.begin(), times_received.end());

				//检查变量范围
				for (int i = 0; i < MsgUpToDown.TIMES.size(); i++)
				{
					if (MsgUpToDown.TIMES[i] < 0 || MsgUpToDown.TIMES[i]>65)//0~65
					{
						printf("\nerror: time out of range.\n");
						exit(1);
					}
				}
				for (int i = 0; i < MsgUpToDown.SPEED.size(); i++)
				{
					if (MsgUpToDown.SPEED[i] < -4500 || MsgUpToDown.SPEED[i]>4500)//-4500~4500
					{
						printf("\nerror: speed out of range.\n");
						exit(1);
					}
				}
				for (int i = 0; i < MsgUpToDown.POSITION.size(); i++)
				{
					if (MsgUpToDown.POSITION[i] < 0 || MsgUpToDown.POSITION[i]>360)//0~360
					{
						printf("\nerror: positon out of range.\n");
						exit(1);
					}
				}

				if (TIMES != current_times)//判定时刻表是否更新
				{
					//时刻表已更新
					//初始化已接收数据
					command_received.clear();
					current_times.clear();
					times_received.clear();
					//重赋时刻表
					current_times.assign(TIMES.begin(), TIMES.end());
					sort(current_times.begin(), current_times.end());
				}

				if (find(current_times.begin(), current_times.end(), time) != current_times.end() && find(times_received.begin(), times_received.end(), time) == times_received.end())
				{
					//当前命令的time属包含于时刻表
					//当前time未收到
					//ROS_INFO("command_recvd %d\n",(int)command_received.size());
					times_received.push_back(time);//加入当前time

					//加入当前命令
					CommandFrame temp;
					temp.ID = MsgUpToDown.ID;
					temp.POSITION = MsgUpToDown.POSITION;
					temp.SPEED = MsgUpToDown.SPEED;
					temp.TIME = MsgUpToDown.TIME;
					temp.TIMES = MsgUpToDown.TIMES;
					command_received.push_back(temp);
				}
				sort(times_received.begin(), times_received.end());
				sort(command_received.begin(), command_received.end(), SortByTime);
				if (TIMES == times_received)
				{
					//数据接收完毕,准备发送

					can_test_02::DownToUp MsgDownToUp;
					MsgDownToUp.ID = -1;
					MsgDownToUp.TYPE = "COMMAND_RECEIVE_COMPLETE";
					MsgDownToUp.DATA = accumulate(TIMES.begin(), TIMES.end(), 0);
					chatter_pub_0.publish(MsgDownToUp);//发送确认收到的MSG

					send_command();//使用CAN总线发送命令
				}

			}
		}
		else if(MsgUpToDown.TIME==-1)//单关节控制模式
		{
			//初始化已接收数据
			command_received.clear();
			current_times.clear();
			times_received.clear();

			//加入当前命令
			CommandFrame temp;
			temp.ID = MsgUpToDown.ID;
			temp.POSITION = MsgUpToDown.POSITION;
			temp.SPEED = MsgUpToDown.SPEED;
			temp.TIME = MsgUpToDown.TIME;
			temp.TIMES = MsgUpToDown.TIMES;
			command_received.push_back(temp);

			send_command_single();
		}
		else if(MsgUpToDown.TIME==0)//单关节控制模式
		{
			//初始化已接收数据
			command_received.clear();
			current_times.clear();
			times_received.clear();

			//加入当前命令
			CommandFrame temp;
			temp.ID = MsgUpToDown.ID;
			temp.POSITION = MsgUpToDown.POSITION;
			temp.SPEED = MsgUpToDown.SPEED;
			temp.TIME = MsgUpToDown.TIME;
			temp.TIMES = MsgUpToDown.TIMES;
			command_received.push_back(temp);

			send_command_speed();
		}*/

	}

private:
	ros::NodeHandle n;
	ros::Publisher chatter_pub_1;
	ros::Publisher chatter_pub_0;
	ros::Subscriber sub;

};
//-----------------接收命令并处理---------------------


//-----------------SDO下发函数---------------------

int send_command_sdo(int ID, int IDX, int DATA)
{
	ROS_INFO("send_command_sdo");
	void *handle;
	char *error;
	handle = dlopen(SOFILE, RTLD_LAZY);
	if (!handle) {
		fprintf(stderr, "%s\n", dlerror());
		exit(1);
	}
	//printf("\nsuccess: load .so file \n");
	typedef ULONG(*fVCI_Transmit)(DWORD DeviceType, DWORD DeviceInd, DWORD CANInd, PVCI_CAN_OBJ pSend, UINT Len);
	fVCI_Transmit VCI_Transmit = NULL;
	VCI_Transmit = (fVCI_Transmit)dlsym(handle, "VCI_Transmit");
	//设定发送数据组
	//int command_num = command_received.size();
	//int jiont_num = command_received[0].ID.size();
	//ROS_INFO("%d\n",(int)command_received.size());
	command_to_send.clear();

	//for (int i = 0; i < command_num; i++)	{

	VCI_CAN_OBJ send[1];

	uint32_t _ID = (((uint32_t)abs(ID)) & 0x07F) | (0x780 & (((uint32_t)11) << 7));	//标准标识符

	uint16_t idx = (uint16_t)abs(IDX);
	
	uint16_t data = (uint16_t)abs(DATA);
	send[0].ID = _ID;
	send[0].SendType = 0;
	send[0].RemoteFlag = 0;
	send[0].ExternFlag = 0;
	send[0].DataLen = 8;
	send[0].Data[0] = 0x20;
	send[0].Data[1] = (uint8_t)(0xff & (idx >> 8));
	send[0].Data[2] = (uint8_t)(0xff & (idx >> 0));
	send[0].Data[3] = (BYTE)((0x00));
	send[0].Data[4] = (uint8_t)(0xff & (data >> 8));
	send[0].Data[5] = (uint8_t)(0xff & (data >> 0));
	send[0].Data[6] = (BYTE)((0x00));
	send[0].Data[7] = (BYTE)((0x00));

	
		int success_frame_number;
		success_frame_number = VCI_Transmit(VCI_USBCAN2, 0, 0, send, 1);
		if (success_frame_number == 1)
		{
			ROS_INFO("success: send command %d\n", success_frame_number);
			command_received.clear();
			current_times.clear();
			times_received.clear();
			return 0;
		}
		else
		{
			ROS_INFO("error: send command error %d\n", success_frame_number);
		}
		
	return 1;
}

//-----------------SDO 下发函数---------------------

//-----------------停止指令下发函数---------------------
int send_command_stop()
{
	//printf(sizeof(double));
	ROS_INFO("send_command");
	void *handle;
	char *error;
	handle = dlopen(SOFILE, RTLD_LAZY);
	if (!handle) {
		fprintf(stderr, "%s\n", dlerror());
		exit(1);
	}
	//printf("\nsuccess: load .so file \n");

	typedef ULONG(*fVCI_Transmit)(DWORD DeviceType, DWORD DeviceInd, DWORD CANInd, PVCI_CAN_OBJ pSend, UINT Len);
	fVCI_Transmit VCI_Transmit = NULL;
	VCI_Transmit = (fVCI_Transmit)dlsym(handle, "VCI_Transmit");

	//设定发送数据组
	//int command_num = command_received.size();
	int jiont_num = command_received[0].ID.size();
	//ROS_INFO("%d\n",(int)command_received.size());
	command_to_send.clear();

	//for (int i = 0; i < command_num; i++)	{
	for (int j = 0; j < jiont_num; j++)
	{
		CanCommandFrame tmp;

		//uint16_t SPEED = (uint16_t)abs(command_received[0].SPEED[j] * 10000);
		//uint32_t POSITION = (uint32_t)abs(command_received[0].POSITION[j] * 10000 * 180 / 3.1415926535897932384);
		//uint16_t CURRENT = (uint16_t)abs(command_received[0].CURRENT * 100);

		uint32_t _ID = (((uint32_t)command_received[0].ID[j]) & 0x07F) | (0x780 & (((uint32_t)3) << 7));	//标准标识符

		tmp.ID = _ID;
		tmp.SendType = 0;
		tmp.RemoteFlag = 0;
		tmp.ExternFlag = 0;
		tmp.DataLen = 8;
		tmp.Data[0] = (BYTE)((0x00 << 5));
		tmp.Data[1] = (BYTE)((0x00));
		tmp.Data[2] = (BYTE)((0x00));
		tmp.Data[3] = (BYTE)((0x00));

		tmp.Data[4] = (BYTE)((0x00));//(BYTE)((0xff00 & SPEED) >> 8);//速度高八位
		tmp.Data[5] = (BYTE)((0x00));//(BYTE)((0x00ff & SPEED));//速度低八位
		tmp.Data[6] = (BYTE)((0x00));//(BYTE)((0xff00 & CURRENT) >> 8);//时间高八位
		tmp.Data[7] = (BYTE)((0x00));//(BYTE)((0x00ff & CURRENT));//时间低八位
												 //tmp.Data[7]=control_back[0];

		command_to_send.push_back(tmp);
	}
	//}
	int length = command_to_send.size();
	VCI_CAN_OBJ send[length];
	for (int i = 0; i < length; i++)
	{


		send[i].ID = command_to_send[i].ID;
		send[i].SendType = command_to_send[i].SendType;
		send[i].RemoteFlag = command_to_send[i].RemoteFlag;
		send[i].ExternFlag = command_to_send[i].ExternFlag;
		send[i].DataLen = command_to_send[i].DataLen;
		send[i].Data[0] = command_to_send[i].Data[0];
		send[i].Data[1] = command_to_send[i].Data[1];
		send[i].Data[2] = command_to_send[i].Data[2];
		send[i].Data[3] = command_to_send[i].Data[3];
		send[i].Data[4] = command_to_send[i].Data[4];
		send[i].Data[5] = command_to_send[i].Data[5];
		send[i].Data[6] = command_to_send[i].Data[6];
		send[i].Data[7] = command_to_send[i].Data[7];
	}

	
		int success_frame_number;
		success_frame_number = VCI_Transmit(VCI_USBCAN2, 0, 0, send, command_to_send.size());
		if (success_frame_number == command_to_send.size())
		{
			ROS_INFO("success: send command %d\n", success_frame_number);
			command_received.clear();
			current_times.clear();
			times_received.clear();
			return 0;
		}
		else
		{
			ROS_INFO("error: send command error %d\n", success_frame_number);
		}

		
	return 1;
}
//-----------------停止指令下发函数---------------------


//-----------------调零指令下发函数---------------------
int send_command_zero()
{
	//printf(sizeof(double));
	ROS_INFO("send_command");
	void *handle;
	char *error;
	handle = dlopen(SOFILE, RTLD_LAZY);
	if (!handle) {
		fprintf(stderr, "%s\n", dlerror());
		exit(1);
	}
	//printf("\nsuccess: load .so file \n");

	typedef ULONG(*fVCI_Transmit)(DWORD DeviceType, DWORD DeviceInd, DWORD CANInd, PVCI_CAN_OBJ pSend, UINT Len);
	fVCI_Transmit VCI_Transmit = NULL;
	VCI_Transmit = (fVCI_Transmit)dlsym(handle, "VCI_Transmit");

	//设定发送数据组
	//int command_num = command_received.size();
	int jiont_num = command_received[0].ID.size();
	//ROS_INFO("%d\n",(int)command_received.size());
	command_to_send.clear();

	//for (int i = 0; i < command_num; i++)	{
	for (int j = 0; j < jiont_num; j++)
	{
		CanCommandFrame tmp;

		//uint16_t SPEED = (uint16_t)abs(command_received[0].SPEED[j] * 10000);
		//uint32_t POSITION = (uint32_t)abs(command_received[0].POSITION[j] * 10000 * 180 / 3.1415926535897932384);
		//uint16_t CURRENT = (uint16_t)abs(command_received[0].CURRENT * 100);

		uint32_t _ID = (((uint32_t)command_received[0].ID[j]) & 0x07F) | (0x780 & (((uint32_t)3) << 7));	//标准标识符

		tmp.ID = _ID;
		tmp.SendType = 0;
		tmp.RemoteFlag = 0;
		tmp.ExternFlag = 0;
		tmp.DataLen = 8;
		tmp.Data[0] = (BYTE)((0x03 << 5));
		tmp.Data[1] = (BYTE)(0x00);//(BYTE)((0xff0000 & POSITION) >> 16);
		tmp.Data[2] = (BYTE)(0x00);//(BYTE)((0x00ff00 & POSITION) >> 8);
		tmp.Data[3] = (BYTE)(0x00);//(BYTE)((0x0000ff & POSITION) >> 0);

		//if (!(SPEED < 0))
		//{
			//tmp.Data[0] = tmp.Data[0] | 0x01;
		//}
		tmp.Data[4] = (BYTE)(0x00);//(BYTE)((0xff00 & SPEED) >> 8);//速度高八位
		tmp.Data[5] = (BYTE)(0x00);//(BYTE)((0x00ff & SPEED));//速度低八位
		tmp.Data[6] = (BYTE)(0x00);//(BYTE)((0xff00 & CURRENT) >> 8);//时间高八位
		tmp.Data[7] = (BYTE)(0x00);//(BYTE)((0x00ff & CURRENT));//时间低八位
												 //tmp.Data[7]=control_back[0];

		command_to_send.push_back(tmp);
	}
	//}
	int length = command_to_send.size();
	VCI_CAN_OBJ send[length];
	for (int i = 0; i < length; i++)
	{


		send[i].ID = command_to_send[i].ID;
		send[i].SendType = command_to_send[i].SendType;
		send[i].RemoteFlag = command_to_send[i].RemoteFlag;
		send[i].ExternFlag = command_to_send[i].ExternFlag;
		send[i].DataLen = command_to_send[i].DataLen;
		send[i].Data[0] = command_to_send[i].Data[0];
		send[i].Data[1] = command_to_send[i].Data[1];
		send[i].Data[2] = command_to_send[i].Data[2];
		send[i].Data[3] = command_to_send[i].Data[3];
		send[i].Data[4] = command_to_send[i].Data[4];
		send[i].Data[5] = command_to_send[i].Data[5];
		send[i].Data[6] = command_to_send[i].Data[6];
		send[i].Data[7] = command_to_send[i].Data[7];
	}

	
		int success_frame_number;
		success_frame_number = VCI_Transmit(VCI_USBCAN2, 0, 0, send, command_to_send.size());
		if (success_frame_number == command_to_send.size())
		{
			ROS_INFO("success: send command %d\n", success_frame_number);
			command_received.clear();
			current_times.clear();
			times_received.clear();
			return 0;
		}
		else
		{
			ROS_INFO("error: send command error %d\n", success_frame_number);
		}

		
	return 1;
}
//-----------------调零指令下发函数---------------------


//-----------------位置控制下发函数---------------------
int send_command_position()
{
	//printf(sizeof(double));
	ROS_INFO("send_command");
	void *handle;
	char *error;
	handle = dlopen(SOFILE, RTLD_LAZY);
	if (!handle) {
		fprintf(stderr, "%s\n", dlerror());
		exit(1);
	}
	//printf("\nsuccess: load .so file \n");

	typedef ULONG(*fVCI_Transmit)(DWORD DeviceType, DWORD DeviceInd, DWORD CANInd, PVCI_CAN_OBJ pSend, UINT Len);
	fVCI_Transmit VCI_Transmit = NULL;
	VCI_Transmit = (fVCI_Transmit)dlsym(handle, "VCI_Transmit");

	//设定发送数据组
	//int command_num = command_received.size();
	int jiont_num = command_received[0].ID.size();
	//ROS_INFO("%d\n",(int)command_received.size());
	command_to_send.clear();

	//for (int i = 0; i < command_num; i++)	{
	for (int j = 0; j < jiont_num; j++)
	{
		CanCommandFrame tmp;

		uint16_t SPEED = (uint16_t)abs(command_received[0].SPEED[j] * 200*180 / PI);

		uint32_t POSITION = (uint32_t)abs((command_received[0].POSITION[j]+PI) * 1000 * 180 / PI);

		//uint16_t CURRENT = (uint16_t)abs(command_received[0].CURRENT[j] * 100);
		//ROS_INFO("speed:%d",SPEED);

		//ROS_INFO("!!!!!!!!!!!!!!!!!!!!!!!speed:%f",command_received[0].SPEED[j]);
		//ROS_INFO("!!!!!!!!!!!!!!!!!!!!11!position:%f",command_received[0].POSITION[j]);

		uint32_t _ID = (((uint32_t)command_received[0].ID[j]) & 0x07F) | (0x780 & (((uint32_t)3) << 7));	//标准标识符

		ROS_INFO("Position:%d",POSITION);
		tmp.ID = _ID;
		tmp.SendType = 0;
		tmp.RemoteFlag = 0;
		tmp.ExternFlag = 0;
		tmp.DataLen = 8;
		tmp.Data[0] = (BYTE)((0x01 << 5));
		tmp.Data[1] = (BYTE)((0xff0000 & POSITION) >> 16);
		tmp.Data[2] = (BYTE)((0x00ff00 & POSITION) >> 8);
		tmp.Data[3] = (BYTE)((0x0000ff & POSITION) >> 0);
		ROS_INFO("temp.data:%d,%d,%d,%d",tmp.Data[0],tmp.Data[1],tmp.Data[2],tmp.Data[3]);

		if ((SPEED < 0))
		{
			tmp.Data[0] = tmp.Data[0] | 0x01;
		}
		tmp.Data[4] = (BYTE)((0xff00 & SPEED) >> 8);//速度高八位
		tmp.Data[5] = (BYTE)((0x00ff & SPEED));//速度低八位
		tmp.Data[6] = (BYTE)(0x00);//(BYTE)((0xff00 & CURRENT) >> 8);//时间高八位
		tmp.Data[7] = (BYTE)(0x00);//(BYTE)((0x00ff & CURRENT));//时间低八位
		ROS_INFO("speed_data:%d,%d",tmp.Data[4],tmp.Data[5]);

											  //tmp.Data[7]=control_back[0];

		command_to_send.push_back(tmp);
	}

	//}
	int length = command_to_send.size();
	VCI_CAN_OBJ send[length];
	for (int i = 0; i < length; i++)
	{


		send[i].ID = command_to_send[i].ID;
		send[i].SendType = command_to_send[i].SendType;
		send[i].RemoteFlag = command_to_send[i].RemoteFlag;
		send[i].ExternFlag = command_to_send[i].ExternFlag;
		send[i].DataLen = command_to_send[i].DataLen;
		send[i].Data[0] = command_to_send[i].Data[0];
		send[i].Data[1] = command_to_send[i].Data[1];
		send[i].Data[2] = command_to_send[i].Data[2];
		send[i].Data[3] = command_to_send[i].Data[3];
		send[i].Data[4] = command_to_send[i].Data[4];
		send[i].Data[5] = command_to_send[i].Data[5];
		send[i].Data[6] = command_to_send[i].Data[6];
		send[i].Data[7] = command_to_send[i].Data[7];
		ROS_INFO("PPPPPPPPPPPPPPPPPPPPPPOS %d\n", (int)command_to_send[i].ID);
	}

	
		int success_frame_number;
		success_frame_number = VCI_Transmit(VCI_USBCAN2, 0, 0, send, command_to_send.size());
		if (success_frame_number == command_to_send.size())
		{
			ROS_INFO("success: send command %d\n", success_frame_number);
			command_received.clear();
			current_times.clear();
			times_received.clear();
			my_test++;
			ROS_INFO("%d,%d\n", my_test, success_frame_number);
			return 0;
		}
		else
		{
			ROS_INFO("error: send command error %d\n", success_frame_number);
		}

	return 1;
}
//-----------------位置控制下发函数---------------------




//-----------------速度控制命令下发函数---------------------
int send_command_speed()
{
	//printf(sizeof(double));
	ROS_INFO("send_command_speed");
	void *handle;
	char *error;
	handle = dlopen(SOFILE, RTLD_LAZY);
	if (!handle) {
		fprintf(stderr, "%s\n", dlerror());
		exit(1);
	}
	//printf("\nsuccess: load .so file \n");

	typedef ULONG(*fVCI_Transmit)(DWORD DeviceType, DWORD DeviceInd, DWORD CANInd, PVCI_CAN_OBJ pSend, UINT Len);
	fVCI_Transmit VCI_Transmit = NULL;
	VCI_Transmit = (fVCI_Transmit)dlsym(handle, "VCI_Transmit");

	//设定发送数据组
	//int command_num = command_received.size();
	int jiont_num = command_received[0].ID.size();
	//ROS_INFO("%d\n",(int)command_received.size());
	command_to_send.clear();

	//for (int i = 0; i < command_num; i++)	{
	for (int j = 0; j < jiont_num; j++)
	{
		CanCommandFrame tmp;

		uint16_t SPEED = (uint16_t)abs(command_received[0].SPEED[j] * 200*180/PI);
		//uint32_t POSITION = (uint32_t)abs(command_received[0].POSITION[j] * 10000 * 180 / 3.1415926535897932384);
		//uint16_t TIME = (uint16_t)abs(command_received[0].TIME * 100);

		uint32_t _ID = (((uint32_t)command_received[0].ID[j]) & 0x07F) | (0x780 & (((uint32_t)3) << 7));	//标准标识符

		tmp.ID = _ID;
		tmp.SendType = 0;
		tmp.RemoteFlag = 0;
		tmp.ExternFlag = 0;
		tmp.DataLen = 8;
		tmp.Data[0] = (BYTE)((0x02 << 5));
		tmp.Data[1] = (BYTE)(0x00);//(BYTE)((0xff0000 & POSITION) >> 16);
		tmp.Data[2] = (BYTE)(0x00);//(BYTE)((0x00ff00 & POSITION) >> 8);
		tmp.Data[3] = (BYTE)(0x00); //(BYTE)((0x0000ff & POSITION) >> 0);

		if ((command_received[0].SPEED[j] < 0))
		{
			tmp.Data[0] = tmp.Data[0] | 0x01;
		}
		tmp.Data[4] = (BYTE)((0xff00 & SPEED) >> 8);//速度高八位
		tmp.Data[5] = (BYTE)((0x00ff & SPEED));//速度低八位
		tmp.Data[6] = (BYTE)(0x00);//(BYTE)((0xff00 & TIME) >> 8);//时间高八位
		tmp.Data[7] = (BYTE)(0x00);//(BYTE)((0x00ff & TIME));//时间低八位
											  //tmp.Data[7]=control_back[0];

		command_to_send.push_back(tmp);
	}
	//}
	int length = command_to_send.size();
	VCI_CAN_OBJ send[length];
	for (int i = 0; i < length; i++)
	{


		send[i].ID = command_to_send[i].ID;
		send[i].SendType = command_to_send[i].SendType;
		send[i].RemoteFlag = command_to_send[i].RemoteFlag;
		send[i].ExternFlag = command_to_send[i].ExternFlag;
		send[i].DataLen = command_to_send[i].DataLen;
		send[i].Data[0] = command_to_send[i].Data[0];
		send[i].Data[1] = command_to_send[i].Data[1];
		send[i].Data[2] = command_to_send[i].Data[2];
		send[i].Data[3] = command_to_send[i].Data[3];
		send[i].Data[4] = command_to_send[i].Data[4];
		send[i].Data[5] = command_to_send[i].Data[5];
		send[i].Data[6] = command_to_send[i].Data[6];
		send[i].Data[7] = command_to_send[i].Data[7];
	}

	
		int success_frame_number;
		success_frame_number = VCI_Transmit(VCI_USBCAN2, 0, 0, send, command_to_send.size());
		if (success_frame_number == command_to_send.size())
		{
			ROS_INFO("success: send command %d\n", success_frame_number);
			command_received.clear();
			current_times.clear();
			times_received.clear();
			return 0;
		}
		else
		{
			ROS_INFO("error: send command error %d\n", success_frame_number);
		}

		
	return 1;
}
//-----------------速度控制命令下发函数---------------------


//-----------------电流控制命令下发函数---------------------
int send_command_current()
{
	//printf(sizeof(double));
	ROS_INFO("send_command_current");
	void *handle;
	char *error;
	handle = dlopen(SOFILE, RTLD_LAZY);
	if (!handle) {
		fprintf(stderr, "%s\n", dlerror());
		exit(1);
	}
	//printf("\nsuccess: load .so file \n");

	typedef ULONG(*fVCI_Transmit)(DWORD DeviceType, DWORD DeviceInd, DWORD CANInd, PVCI_CAN_OBJ pSend, UINT Len);
	fVCI_Transmit VCI_Transmit = NULL;
	VCI_Transmit = (fVCI_Transmit)dlsym(handle, "VCI_Transmit");

	//设定发送数据组
	//int command_num = command_received.size();
	int jiont_num = command_received[0].ID.size();
	//ROS_INFO("%d\n",(int)command_received.size());
	command_to_send.clear();

	//for (int i = 0; i < command_num; i++)	{
	for (int j = 0; j < jiont_num; j++)
	{
		CanCommandFrame tmp;

		uint16_t CURRENT = (uint16_t)abs(command_received[0].CURRENT[j] *65535.0/5.0);
		//uint32_t POSITION = (uint32_t)abs(command_received[0].POSITION[j] * 10000 * 180 / 3.1415926535897932384);
		//uint16_t TIME = (uint16_t)abs(command_received[0].TIME * 100);

		uint32_t _ID = (((uint32_t)command_received[0].ID[j]) & 0x07F) | (0x780 & (((uint32_t)3) << 7));	//标准标识符

		tmp.ID = _ID;
		tmp.SendType = 0;
		tmp.RemoteFlag = 0;
		tmp.ExternFlag = 0;
		tmp.DataLen = 8;
		tmp.Data[0] = (BYTE)((0x06 << 5));
		tmp.Data[1] = (BYTE)(0x00);//(BYTE)((0xff0000 & POSITION) >> 16);
		tmp.Data[2] = (BYTE)(0x00);//(BYTE)((0x00ff00 & POSITION) >> 8);
		tmp.Data[3] = (BYTE)(0x00); //(BYTE)((0x0000ff & POSITION) >> 0);

		
		tmp.Data[4] = (BYTE)(0x00);//(BYTE)((0xff00 & SPEED) >> 8);//速度高八位
		tmp.Data[5] = (BYTE)(0x00);//(BYTE)((0x00ff & SPEED));//速度低八位
		tmp.Data[6] = (BYTE)((0xff00 & CURRENT) >> 8);//电流高八位
		tmp.Data[7] = (BYTE)((0x00ff & CURRENT));//电流低八位
											  //tmp.Data[7]=control_back[0];

		if ((command_received[0].CURRENT[j] < 0))
		{
			tmp.Data[0] = tmp.Data[0] | 0x02;
		}


ROS_INFO("temp.data:%d,%d,%d,%d,%d,%d,%d,%d",tmp.Data[0],tmp.Data[1],tmp.Data[2],tmp.Data[3],tmp.Data[4],tmp.Data[1],tmp.Data[2],tmp.Data[3]);


		command_to_send.push_back(tmp);
	}
	//}
	int length = command_to_send.size();
	VCI_CAN_OBJ send[length];
	for (int i = 0; i < length; i++)
	{


		send[i].ID = command_to_send[i].ID;
		send[i].SendType = command_to_send[i].SendType;
		send[i].RemoteFlag = command_to_send[i].RemoteFlag;
		send[i].ExternFlag = command_to_send[i].ExternFlag;
		send[i].DataLen = command_to_send[i].DataLen;
		send[i].Data[0] = command_to_send[i].Data[0];
		send[i].Data[1] = command_to_send[i].Data[1];
		send[i].Data[2] = command_to_send[i].Data[2];
		send[i].Data[3] = command_to_send[i].Data[3];
		send[i].Data[4] = command_to_send[i].Data[4];
		send[i].Data[5] = command_to_send[i].Data[5];
		send[i].Data[6] = command_to_send[i].Data[6];
		send[i].Data[7] = command_to_send[i].Data[7];
	}

	
		int success_frame_number;
		success_frame_number = VCI_Transmit(VCI_USBCAN2, 0, 0, send, command_to_send.size());
		if (success_frame_number == command_to_send.size())
		{
			ROS_INFO("success: send command %d\n", success_frame_number);
			command_received.clear();
			current_times.clear();
			times_received.clear();
			return 0;
		}
		else
		{
			ROS_INFO("error: send command error %d\n", success_frame_number);
		}

		
	return 1;
}
//-----------------电流控制命令下发函数---------------------



//-----------------速度直接控制命令下发函数---------------------
int send_command_speed_direct()
{
	//printf(sizeof(double));
	ROS_INFO("send_command_speed");
	void *handle;
	char *error;
	handle = dlopen(SOFILE, RTLD_LAZY);
	if (!handle) {
		fprintf(stderr, "%s\n", dlerror());
		exit(1);
	}
	//printf("\nsuccess: load .so file \n");

	typedef ULONG(*fVCI_Transmit)(DWORD DeviceType, DWORD DeviceInd, DWORD CANInd, PVCI_CAN_OBJ pSend, UINT Len);
	fVCI_Transmit VCI_Transmit = NULL;
	VCI_Transmit = (fVCI_Transmit)dlsym(handle, "VCI_Transmit");

	//设定发送数据组
	//int command_num = command_received.size();
	int jiont_num = command_received[0].ID.size();
	//ROS_INFO("%d\n",(int)command_received.size());
	command_to_send.clear();

	//for (int i = 0; i < command_num; i++)	{
	for (int j = 0; j < jiont_num; j++)
	{
		CanCommandFrame tmp;

		uint16_t SPEED = (uint16_t)abs(command_received[0].SPEED[j] * 200 * 180 / PI);
		//uint32_t POSITION = (uint32_t)abs(command_received[0].POSITION[j] * 10000 * 180 / 3.1415926535897932384);
		//uint16_t TIME = (uint16_t)abs(command_received[0].TIME * 100);

		uint32_t _ID = (((uint32_t)command_received[0].ID[j]) & 0x07F) | (0x780 & (((uint32_t)3) << 7));	//标准标识符

		tmp.ID = _ID;
		tmp.SendType = 0;
		tmp.RemoteFlag = 0;
		tmp.ExternFlag = 0;
		tmp.DataLen = 8;
		tmp.Data[0] = (BYTE)((0x04 << 5));
		tmp.Data[1] = (BYTE)(0x00);//(BYTE)((0xff0000 & POSITION) >> 16);
		tmp.Data[2] = (BYTE)(0x00);//(BYTE)((0x00ff00 & POSITION) >> 8);
		tmp.Data[3] = (BYTE)(0x00); //(BYTE)((0x0000ff & POSITION) >> 0);

		if ((command_received[0].SPEED[j] < 0))
		{
			tmp.Data[0] = tmp.Data[0] | 0x01;
		}
		
		ROS_INFO("success: send command SPEED                      %d\n", SPEED);

		tmp.Data[4] = (BYTE)((0xff00 & SPEED) >> 8);//速度高八位
		tmp.Data[5] = (BYTE)((0x00ff & SPEED));//速度低八位
		tmp.Data[6] = (BYTE)(0x00);//(BYTE)((0xff00 & TIME) >> 8);//时间高八位
		tmp.Data[7] = (BYTE)(0x00);//(BYTE)((0x00ff & TIME));//时间低八位
											  //tmp.Data[7]=control_back[0];

		command_to_send.push_back(tmp);
	}
	//}
	int length = command_to_send.size();
	VCI_CAN_OBJ send[length];
	for (int i = 0; i < length; i++)
	{


		send[i].ID = command_to_send[i].ID;
		send[i].SendType = command_to_send[i].SendType;
		send[i].RemoteFlag = command_to_send[i].RemoteFlag;
		send[i].ExternFlag = command_to_send[i].ExternFlag;
		send[i].DataLen = command_to_send[i].DataLen;
		send[i].Data[0] = command_to_send[i].Data[0];
		send[i].Data[1] = command_to_send[i].Data[1];
		send[i].Data[2] = command_to_send[i].Data[2];
		send[i].Data[3] = command_to_send[i].Data[3];
		send[i].Data[4] = command_to_send[i].Data[4];
		send[i].Data[5] = command_to_send[i].Data[5];
		send[i].Data[6] = command_to_send[i].Data[6];
		send[i].Data[7] = command_to_send[i].Data[7];
	}

	
		int success_frame_number;
		success_frame_number = VCI_Transmit(VCI_USBCAN2, 0, 0, send, command_to_send.size());
		if (success_frame_number == command_to_send.size())
		{
			ROS_INFO("success: send command %d\n", success_frame_number);
			command_received.clear();
			current_times.clear();
			times_received.clear();
			return 0;
		}
		else
		{
			ROS_INFO("error: send command error %d\n", success_frame_number);
		}

		
	return 1;
}
//-----------------速度直接控制命令下发函数---------------------

/////////////////
int send_command_position_PID()
{
		//printf(sizeof(double));
	ROS_INFO("send_command_position_PID");
	void *handle;
	char *error;
	handle = dlopen(SOFILE, RTLD_LAZY);
	if (!handle) {
		fprintf(stderr, "%s\n", dlerror());
		exit(1);
	}
	//printf("\nsuccess: load .so file \n");

	typedef ULONG(*fVCI_Transmit)(DWORD DeviceType, DWORD DeviceInd, DWORD CANInd, PVCI_CAN_OBJ pSend, UINT Len);
	fVCI_Transmit VCI_Transmit = NULL;
	VCI_Transmit = (fVCI_Transmit)dlsym(handle, "VCI_Transmit");

	//设定发送数据组
	//int command_num = command_received.size();
	//ROS_INFO("0");
	int jiont_num = command_received[0].ID.size();
	//ROS_INFO("%d\n",(int)command_received.size());
	command_to_send.clear();

	//ROS_INFO("1");

	//for (int i = 0; i < command_num; i++)	{
	for (int j = 0; j < 1; j++)
	{
		CanCommandFrame tmp;

		//uint16_t SPEED = (uint16_t)abs(command_received[0].SPEED[j] * 10000);
		//uint32_t POSITION = (uint32_t)abs(command_received[0].POSITION[j] * 10000 * 180 / 3.1415926535897932384);
		//uint16_t TIME = (uint16_t)abs(command_received[0].TIME * 100);
		//ROS_INFO("2");
		uint32_t _ID = (((uint32_t)command_received[0].ID[j]) & 0x07F) | (0x780 & (((uint32_t)3) << 7));	//标准标识符

		tmp.ID = _ID;
		tmp.SendType = 0;
		tmp.RemoteFlag = 0;
		tmp.ExternFlag = 0;
		tmp.DataLen = 8;
		tmp.Data[0] = (BYTE)((0x05 << 5));
		tmp.Data[1] = (BYTE)(0x00);//(BYTE)((0xff0000 & POSITION) >> 16);
		tmp.Data[2] = (BYTE)(0x00);//(BYTE)((0x00ff00 & POSITION) >> 8);
		tmp.Data[3] = (BYTE)(0x00); //(BYTE)((0x0000ff & POSITION) >> 0);
		tmp.Data[4] = (BYTE)(0x00);//(BYTE)((0x00 & SPEED) >> 8);//速度高八位
		tmp.Data[5] = (BYTE)(0x00);// (BYTE)((0x00ff & SPEED));//速度低八位
		tmp.Data[6] = (BYTE)(0x00);//(BYTE)((0xff00 & TIME) >> 8);//时间高八位
		tmp.Data[7] = (BYTE)(0x00);//(BYTE)((0x00ff & TIME));//时间低八位
											  //tmp.Data[7]=control_back[0];
		//ROS_INFO("send_command_position_PID %d : %d %d %d %d %d %d %d %d ",_ID,tmp.Data[0],tmp.Data[1],tmp.Data[2],tmp.Data[3],tmp.Data[4],tmp.Data[5],tmp.Data[6],tmp.Data[7]);
		command_to_send.push_back(tmp);
	}
	//}
	int length = 1;//command_to_send.size();
	VCI_CAN_OBJ send[length];

	//ROS_INFO("3");
	for (int i = 0; i < length; i++)
	{


		send[i].ID = command_to_send[i].ID;
		send[i].SendType = command_to_send[i].SendType;
		send[i].RemoteFlag = command_to_send[i].RemoteFlag;
		send[i].ExternFlag = command_to_send[i].ExternFlag;
		send[i].DataLen = command_to_send[i].DataLen;
		send[i].Data[0] = command_to_send[i].Data[0];
		send[i].Data[1] = command_to_send[i].Data[1];
		send[i].Data[2] = command_to_send[i].Data[2];
		send[i].Data[3] = command_to_send[i].Data[3];
		send[i].Data[4] = command_to_send[i].Data[4];
		send[i].Data[5] = command_to_send[i].Data[5];
		send[i].Data[6] = command_to_send[i].Data[6];
		send[i].Data[7] = command_to_send[i].Data[7];
	}

	
		int success_frame_number;
		success_frame_number = VCI_Transmit(VCI_USBCAN2, 0, 0, send, command_to_send.size());
		if (success_frame_number == command_to_send.size())
		{
			ROS_INFO("success: send command %d\n", success_frame_number);
			command_received.clear();
			current_times.clear();
			times_received.clear();
			return 0;
		}
		else
		{
			ROS_INFO("error: send command error %d\n", success_frame_number);
		}

		
	return 1;
}

//-----------------MAIN---------------------
int main(int argc, char **argv)
{
	//从.so中加载CAN通讯控制函数
	void *handle;
	char *error;
	handle = dlopen(SOFILE, RTLD_LAZY);
	if (!handle) {
		fprintf(stderr, "%s\n", dlerror());
		printf("\nerror: load .so file \n");
		exit(1);
	}
	//printf("\nsuccess: load .so file \n");

	typedef DWORD(*fVCI_OpenDevice)(DWORD DeviceType, DWORD DeviceInd, DWORD Reserved);
	fVCI_OpenDevice VCI_OpenDevice = NULL;
	VCI_OpenDevice = (fVCI_OpenDevice)dlsym(handle, "VCI_OpenDevice");

	typedef ULONG(*fVCI_Receive)(DWORD DeviceType, DWORD DeviceInd, DWORD CANInd, PVCI_CAN_OBJ pReceive, UINT Len, INT WaitTime);
	fVCI_Receive VCI_Receive = NULL;
	VCI_Receive = (fVCI_Receive)dlsym(handle, "VCI_Receive");

	typedef DWORD(*fVCI_InitCAN)(DWORD DeviceType, DWORD DeviceInd, DWORD CANInd, PVCI_INIT_CONFIG pInitConfig);
	fVCI_InitCAN VCI_InitCAN = NULL;
	VCI_InitCAN = (fVCI_InitCAN)dlsym(handle, "VCI_InitCAN");

	typedef DWORD(*fVCI_StartCAN)(DWORD DeviceType, DWORD DeviceInd, DWORD CANInd);
	fVCI_StartCAN VCI_StartCAN = NULL;
	VCI_StartCAN = (fVCI_StartCAN)dlsym(handle, "VCI_StartCAN");

	typedef DWORD(*fVCI_CloseDevice)(DWORD DeviceType, DWORD DeviceInd);
	fVCI_CloseDevice VCI_CloseDevice = NULL;
	VCI_CloseDevice = (fVCI_CloseDevice)dlsym(handle, "VCI_CloseDevice");



	if ((error = dlerror()) != NULL) {
		fprintf(stderr, "%s\n", error);
		exit(1);
	} /**/

	//printf("success: load functions \n");


	//初始化CAN通讯硬件
	if (VCI_OpenDevice(VCI_USBCAN2, 0, 0) != 1)
	{
		printf("open deivce error\n");
		exit(1);
	}/**/


	VCI_INIT_CONFIG config;
	config.AccCode = 0;
	config.AccMask = 0xffffffff;
	config.Filter = 1;
	config.Mode = 0;

	/*波特率 500K  0x00  0x1C*/
	config.Timing0 = 0x00;
	config.Timing1 = 0x1C;

	if (VCI_InitCAN(VCI_USBCAN2, 0, 0, &config) != 1)
	{
		printf("init CAN error\n");
		VCI_CloseDevice(VCI_USBCAN2, 0);
		exit(1);
	}

	if (VCI_StartCAN(VCI_USBCAN2, 0, 0) != 1)
	{
		printf("Start CAN error\n");
		VCI_CloseDevice(VCI_USBCAN2, 0);
		exit(1);
	}

	ros::init(argc, argv, "driver_can");

	ros::NodeHandle n;

	//ros::Publisher chatter_pub = n.advertise<std_msgs::String>("Comm_Recv", 1000);


  //FollowJointTrajectoryAction followJointTrajectory("follow_joint_trajectory");//moveit

	//ros::Publisher chatter_pub_0 = n.advertise<sensor_msgs::JointState>("joint_states", 1000);
	ros::Publisher chatter_pub_1 = n.advertise<can_test_02::orig_new>("Comm_Orig", 1000);
	ros::Publisher chatter_pub_0 = n.advertise<can_test_02::DownToUp>("down_to_up", 1000);

	CommandReceiver command_receiver;//监视Topic中命令

	//ros::Publisher comm_recv_pub = n.advertise<beginner_tutorials::recv>("Comm_Recv", 1000);
	//ros::Subscriber sub = n.subscribe("up_to_down", 1000, chatterCallback);
	ros::Rate loop_rate(500);

	/**
	 * A count of how many messages we have sent. This is used to create
	 * a unique string for each message.
	 */
	int count = 0;

	int reclen = 0;
	VCI_CAN_OBJ rec[100];
	int i;
	int ind = 0;
	//int *run=(int*)param;
	int framenum;

	while (ros::ok())
	{

		//printf("running....%d\n",ind);
		reclen = VCI_Receive(VCI_USBCAN2, 0, ind, rec, 2500, 0);
		if (reclen > 0)
		{

			ROS_INFO("%d frames received",reclen);

			for (framenum = 0; framenum < reclen; framenum++)
			{
				can_test_02::orig_new OrigComm;
				OrigComm.ID = rec[framenum].ID&0x7F;
				OrigComm.FUNC = rec[framenum].ID>>7;
				for (i = 0; i < rec[framenum].DataLen; i++)
				{
					//ss <<setw(2)<<hex<<(int)rec[reclen-1].Data[i]<<" ";
					OrigComm.DATA[i] = (int)rec[framenum].Data[i];
				}/**/

				chatter_pub_1.publish(OrigComm);
				++count;
				//ROS_INFO("frame received\n");
				//------------------判断帧功能类型-----------------------
				uint8_t _mode = (uint8_t)(rec[framenum].ID >> 7);
				if (_mode == 0)
				{
					//ROS_INFO("psc frame received\n");
					//判定为运动状态电流帧
					int flag_spd;
					int flag_cur;
					can_test_02::DownToUp MsgDownToUp;
					MsgDownToUp.ID = (rec[framenum].ID)&(0x07f);

					MsgDownToUp.TYPE = "POSITION";
					int tmppos;
					int tmpspd;
					int tmpcur;

					tmppos = tmppos & 0x00000000;
					//tmppos = tmppos | (0xff0000 & ((unsigned int)rec[framenum].Data[1] << 16));
					//pos1&0xff00ff;
					//tmppos = tmppos | (0x00ff00 & ((unsigned int)rec[framenum].Data[2] << 8));
					//pos1&0xffff00;
					//tmppos = tmppos | (0x0000ff & ((unsigned int)rec[framenum].Data[3] << 0));
					tmppos = ((int)rec[framenum].Data[1])*256*256 + ((int)rec[framenum].Data[2])*256 + (int)rec[framenum].Data[3];


					//ROS_INFO("tmppos:%d",tmppos);
					//ROS_INFO("rec.data:%d,%d,%d",rec[framenum].Data[1],rec[framenum].Data[2],rec[framenum].Data[3]);
						
					//ROS_INFO("int tmppos:%d",(int)tmppos);


					MsgDownToUp.DATA.resize(1);
					MsgDownToUp.DATA[0] = 0-PI+((double)((int)tmppos))*PI/180/1000;
/*
if (MsgDownToUp.DATA[0]<-0.5)
{
ROS_INFO("rec.data:%d,%d,%d",rec[framenum].Data[1],rec[framenum].Data[2],rec[framenum].Data[3]);
ROS_INFO("DATA.DATA:%f",MsgDownToUp.DATA[0]);
exit(1);
}
*/

					chatter_pub_0.publish(MsgDownToUp);
					//ROS_INFO("double tmppos:%f",(double)((int)tmppos));

					MsgDownToUp.TYPE = "SPEED";
					tmpspd = ((int)rec[framenum].Data[4])*256 + (int)rec[framenum].Data[5];
					
					if (!(0x01 & ((unsigned int)rec[framenum].Data[0] >> 0))) {
				flag_spd = 1;
			}
			else {
				flag_spd = -1;
			}
					MsgDownToUp.DATA.resize(1);
					MsgDownToUp.DATA[0] = (double)(flag_spd*tmpspd)*PI/180/200;
					chatter_pub_0.publish(MsgDownToUp);

					
					MsgDownToUp.TYPE = "CURRENT";
			
			tmpcur = ((int)rec[framenum].Data[6])*256 + (int)rec[framenum].Data[7];
					if (!(0x01 & ((unsigned int)rec[framenum].Data[0] >> 1))) {
				flag_cur = 1;
			}
			else {
				flag_cur = -1;
			}
					MsgDownToUp.DATA.resize(1);
					MsgDownToUp.DATA[0] = (double)(flag_cur*tmpcur)*5/65535;
					chatter_pub_0.publish(MsgDownToUp);

					continue;
				}
				else if (_mode == 1)
				{
					//判定为温度帧
					can_test_02::DownToUp MsgDownToUp;
					MsgDownToUp.ID = (rec[framenum].ID)&(0x07f);

					MsgDownToUp.TYPE = "TEMPERATURE";
					MsgDownToUp.DATA.resize(1);
					MsgDownToUp.DATA[0] = (double)(((short)(rec[framenum].Data[0]<<8 | rec[framenum].Data[1])))/340.0+36.25; 
					chatter_pub_0.publish(MsgDownToUp);


					continue;
				}
				else if (_mode == 2)
				{
					//判定为加速度帧
					//			angular_acc[2][i] = ((short)(MPU6050_data[7][i]<<8 | MPU6050_data[6][i]))/32768.0*16;      //ZÖá¼ÓËÙ¶È
					can_test_02::DownToUp MsgDownToUp;
					MsgDownToUp.ID = (rec[framenum].ID)&(0x07f);
					MsgDownToUp.DATA.resize(3);

					MsgDownToUp.TYPE = "ACCELERATION";
					MsgDownToUp.DATA[0] = (double)(((short)(rec[framenum].Data[0]<<8 | rec[framenum].Data[1]))) / 32768.0 * 16;
					MsgDownToUp.DATA[1] = (double)(((short)(rec[framenum].Data[2]<<8 | rec[framenum].Data[3]))) / 32768.0 * 16;
					MsgDownToUp.DATA[2] = (double)(((short)(rec[framenum].Data[4]<<8 | rec[framenum].Data[5]))) / 32768.0 * 16;
					chatter_pub_0.publish(MsgDownToUp);

					continue;
				}
				else if (_mode == 3)
				{
					//判定为角速度帧
					//			angular_ver[0][i] = ((short)(MPU6050_data[3][i]<<8| MPU6050_data[2][i]))/32768.0*2000;      //XÖá½ÇËÙ¶È
					can_test_02::DownToUp MsgDownToUp;
					MsgDownToUp.ID = (rec[framenum].ID)&(0x07f);
					MsgDownToUp.DATA.resize(3);

					MsgDownToUp.TYPE = "ANGULAR_SPEED";
					MsgDownToUp.DATA[0] = (double)(((short)(rec[framenum].Data[0]<<8 | rec[framenum].Data[1]))) / 32768.0 * 2000;
					MsgDownToUp.DATA[1] = (double)(((short)(rec[framenum].Data[2]<<8 | rec[framenum].Data[3]))) / 32768.0 * 2000;
					MsgDownToUp.DATA[2] = (double)(((short)(rec[framenum].Data[4]<<8 | rec[framenum].Data[5]))) / 32768.0 * 2000;
					chatter_pub_0.publish(MsgDownToUp);

					continue;
				}
				else if (_mode == 4)
				{
					//判定为姿态角帧
					can_test_02::DownToUp MsgDownToUp;
					MsgDownToUp.ID = (rec[framenum].ID)&(0x07f);
					MsgDownToUp.DATA.resize(3);

					MsgDownToUp.TYPE = "RPY_ANGLE";

					//			angular[2][i] = ((short)(MPU6050_data[7][i]<<8| MPU6050_data[6][i]))/32768.0*180;   //ZÖáÆ«º½½Ç£¨z Öá£

					MsgDownToUp.DATA[0] = (double)(((short)(rec[framenum].Data[0]<<8 | rec[framenum].Data[1]))) / 32768.0 * 180;
					MsgDownToUp.DATA[1] = (double)(((short)(rec[framenum].Data[2]<<8 | rec[framenum].Data[3]))) / 32768.0 * 180;
					MsgDownToUp.DATA[2] = (double)(((short)(rec[framenum].Data[4]<<8 | rec[framenum].Data[5]))) / 32768.0 * 180;
					chatter_pub_0.publish(MsgDownToUp);

					continue;
				}
				else if (_mode == 5)

				{
					//判定为关节硬件ID帧
					can_test_02::DownToUp MsgDownToUp;
					MsgDownToUp.ID = (rec[framenum].ID)&(0x07f);
					MsgDownToUp.DATA.resize(1);

					MsgDownToUp.TYPE = "HARDWARE_ID";
					int sum = 0;
					for(int i=0;i<2;i++)
					{
						sum = sum + rec[framenum].Data[i] * pow(256,1-i);
					}
					MsgDownToUp.DATA[0] = sum;
					chatter_pub_0.publish(MsgDownToUp);

					continue;
				}
				else if (_mode == 6)
				{
					//判定为关节速度PID帧
					can_test_02::DownToUp MsgDownToUp;
					MsgDownToUp.ID = (rec[framenum].ID)&(0x07f);
					MsgDownToUp.DATA.resize(3);

					MsgDownToUp.TYPE = "SPEED_PID";
					MsgDownToUp.DATA[0] = (rec[framenum].Data[0]*256+rec[framenum].Data[1])/10000;
					MsgDownToUp.DATA[1] = (rec[framenum].Data[2]*256+rec[framenum].Data[3])/10000;
					MsgDownToUp.DATA[2] = (rec[framenum].Data[4]*256+rec[framenum].Data[5])/10000;
					chatter_pub_0.publish(MsgDownToUp);

					continue;
				}
				else if (_mode == 7)
				{
					//判定为关节位置PID帧
					can_test_02::DownToUp MsgDownToUp;
					MsgDownToUp.ID = (rec[framenum].ID)&(0x07f);
					MsgDownToUp.DATA.resize(3);

					MsgDownToUp.TYPE = "POSITION_PID";
					MsgDownToUp.DATA[0] = (rec[framenum].Data[0]*256+rec[framenum].Data[1])/10;
					MsgDownToUp.DATA[1] = (rec[framenum].Data[2]*256+rec[framenum].Data[3])/10;
					MsgDownToUp.DATA[2] = (rec[framenum].Data[4]*256+rec[framenum].Data[5])/10;
					chatter_pub_0.publish(MsgDownToUp);
					continue;
				}
				else
				{
					//不符合现有的任何标志,判定为无效帧
					continue;
				}

			}

		}


		//--------------------ROS循环-----------------
		ros::spinOnce();
		loop_rate.sleep();


	}
	VCI_CloseDevice(VCI_USBCAN2, 0);
	dlclose(handle);
	return 0;
}
//-----------------MAIN---------------------

