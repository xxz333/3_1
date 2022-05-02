//采用垂直划分的方式，对于内层循环进行拆分，即每个线程负责若干列
#include<iostream>
#include<pthread.h>
#include<sys/time.h>
#include <semaphore.h>
//#include<arm_neon.h>

using namespace std;

const int worker_count=8;
int n;
float** A;
struct timeval val;
struct timeval newval;

//线性数据结构定义
typedef struct
{
    int t_id;
}threadParam_t;

//信号量定义
sem_t sem_leader;
sem_t sem_Division[worker_count-1];
sem_t sem_Elimination[worker_count-1];

//线程函数定义（块划分）
void *threadFunc(void *param);
void pthread_static();
//循环划分
void *threadFunc0(void *param);
void pthread_static0();

//垂直划分（垂直划分）
//线程函数定义
void *threadFunc1(void *param)
{
    threadParam_t *p=(threadParam_t*)param;
    int t_id=p->t_id;
    for(int k=0;k<n;k++)
    {
        if(t_id==0)//主线程进行除法操作
        {
            for(int j=k+1;j<n;j++)
                A[k][j]=A[k][j]/A[k][k];
            A[k][k]=1.0;
        }
        else
            sem_wait(&sem_Division[t_id-1]);//阻塞，等待完成除法操作
        
        //t_id为0的线程唤醒其他工作线程，进行消去操作
        if(t_id==0)
        {
            for(int i=0;i<worker_count-1;i++)
                sem_post(&sem_Division[i]);
        }

        // //循环划分任务(水平划分)
        // for(int i=k+1+t_id;i<n;i+=worker_count)
        // {
        //     for(int j=k+1;j<n;j++)
        //         A[i][j]=A[i][j]-A[i][k]*A[k][j];
        //     A[i][k]=0.0;
        // }

        //循环划分任务(垂直划分)
        int num=(n-k)/worker_count;
        int my_first=k+1+t_id*num;
        int my_last;
        if(t_id==worker_count-1)
            my_last=n;
        else
            my_last=my_first+num;
        for(int i=k+1;i<n;i++)
        {
            for(int j=my_first;j<my_last;j++)
                A[i][j]=A[i][j]-A[i][k]*A[k][j];
            A[i][k]=0.0;
        }

        if(t_id==0)
        {
            for(int i=0;i<worker_count-1;i++)
                sem_wait(&sem_leader);//等待其他worker完成消去
            for(int i=0;i<worker_count-1;i++)
                sem_post(&sem_Elimination[i]);//通知其他worker进入下一轮
        }
        else
        {
            sem_post(&sem_leader);//通知leader，已完成消去任务
            sem_wait(&sem_Elimination[t_id-1]);//等待通知，进入下一轮
        }
    }
    pthread_exit(NULL);
}

//垂直划分（循环划分）
//线程函数定义
void *threadFunc3(void *param)
{
    threadParam_t *p=(threadParam_t*)param;
    int t_id=p->t_id;
    for(int k=0;k<n;k++)
    {
        if(t_id==0)
        {
            for(int j=k+1;j<n;j++)
                A[k][j]=A[k][j]/A[k][k];
            A[k][k]=1.0;
        }
        else
            sem_wait(&sem_Division[t_id-1]);
        
        if(t_id==0)
        {
            for(int i=0;i<worker_count-1;i++)
                sem_post(&sem_Division[i]);
        }

        for(int i=k+1+t_id;i<n;i++)
        {
            for(int j=k+1;j<n;j+=worker_count)
                A[i][j]=A[i][j]-A[i][k]*A[k][j];
            A[i][k]=0.0;
        }


        if(t_id==0)
        {
            for(int i=0;i<worker_count-1;i++)
                sem_wait(&sem_leader);
            for(int i=0;i<worker_count-1;i++)
                sem_post(&sem_Elimination[i]);
        }
        else
        {
            sem_post(&sem_leader);
            sem_wait(&sem_Elimination[t_id-1]);
        }
    }
    pthread_exit(NULL);
}

void pthread_vertical()
{
    //初始化信号量
    sem_init(&sem_leader,0,0);
    for(int i=0;i<worker_count-1;i++)
    {
        sem_init(&sem_Division[i],0,0);
        sem_init(&sem_Elimination[i],0,0);
    }
    //创建线程
    pthread_t handles[worker_count];
    threadParam_t param[worker_count];
    for(int t_id=0;t_id<worker_count;t_id++)
    {
        param[t_id].t_id=t_id;
        pthread_create(&handles[t_id],NULL,threadFunc1,(void*)(param+t_id));
    }
    for(int t_id=0;t_id<worker_count;t_id++)
        pthread_join(handles[t_id],NULL);
    //销毁所有信号量
    sem_destroy(&sem_leader);
    for(int i=0;i<worker_count-1;i++)
    {
        sem_destroy(&sem_Division[i]);
        sem_destroy(&sem_Elimination[i]);
    }
}
//垂直划分（循环划分）
void pthread_vertical1()
{

    sem_init(&sem_leader,0,0);
    for(int i=0;i<worker_count-1;i++)
    {
        sem_init(&sem_Division[i],0,0);
        sem_init(&sem_Elimination[i],0,0);
    }

    pthread_t handles[worker_count];
    threadParam_t param[worker_count];
    for(int t_id=0;t_id<worker_count;t_id++)
    {
        param[t_id].t_id=t_id;
        pthread_create(&handles[t_id],NULL,threadFunc3,(void*)(param+t_id));
    }
    for(int t_id=0;t_id<worker_count;t_id++)
        pthread_join(handles[t_id],NULL);

    sem_destroy(&sem_leader);
    for(int i=0;i<worker_count-1;i++)
    {
        sem_destroy(&sem_Division[i]);
        sem_destroy(&sem_Elimination[i]);
    }
}


void m_reset()
{
	for(int i=0;i<n;i++){
		for(int j=0;j<i;j++)
			A[i][j]=0;
		A[i][i]=1.0;
		for(int j=i+1;j<n;j++)
			A[i][j]=rand()%10;
	}
	for(int k=0;k<n;k++)
		for(int i=k+1;i<n;i++)
			for(int j=0;j<n;j++)
				A[i][j]+=A[k][j];
}

void print_result()
{
	for(int i=0;i<n;i++)
    {
        for(int j=0;j<n;j++)
			cout<<A[i][j]<<" ";
		cout<<endl;
    }		
}

int main()
{
    int step=10;
    for(;n<=2000;n+=step)
    {
    double time_2_1=0.0,time_2_2=0.0,time_3_1=0.0,time_3_2=0.0;
    cin>>n;  //矩阵规模
	for(int i=0;i<n;i++)
		A = new float*[n];
    for(int i=0;i<n;i++)
		A[i] = new float[n];
    //循环划分
    m_reset();
    // print_result();
    // cout<<endl;
    gettimeofday(&val,NULL);
    for(int i=0;i<10;i++)
        pthread_static0();
    gettimeofday(&newval,NULL);
    time_2_1+=(newval.tv_sec - val.tv_sec) + (double)(newval.tv_usec - val.tv_usec) / 1000000.0;
    //块划分
    m_reset();
    // print_result();
    // cout<<endl;
    gettimeofday(&val,NULL);
    for(int i=0;i<10;i++)
        pthread_static();
    gettimeofday(&newval,NULL);
    time_2_2+=(newval.tv_sec - val.tv_sec) + (double)(newval.tv_usec - val.tv_usec) / 1000000.0;
    //print_result();

    //垂直划分（循环划分）
    m_reset();
    gettimeofday(&val,NULL);
    for(int i=0;i<10;i++)
        pthread_vertical1();
    gettimeofday(&newval,NULL);
    time_3_2+=(newval.tv_sec - val.tv_sec) + (double)(newval.tv_usec - val.tv_usec) / 1000000.0;

    //垂直划分（块划分）
    m_reset();
    // print_result();
    // cout<<endl;
    gettimeofday(&val,NULL);
    for(int i=0;i<10;i++)
        pthread_vertical();
    gettimeofday(&newval,NULL);
    time_3_1+=(newval.tv_sec - val.tv_sec) + (double)(newval.tv_usec - val.tv_usec) / 1000000.0;
    //print_result();

    for(int i=0;i<n;i++)
        delete A[i];
    delete A;
    cout<<"        "<<n<<"        "<<"& "<<time_2_1<<"   "<<"& "<<time_2_2<<"   "<<"& "<<time_3_1<<" "<<R"(\\ \hline)"<<endl;
    if(n==100){step=100;}
    if(n==1000){step=1000;}
    }
    return 0;
}





//线程函数定义
void *threadFunc0(void *param)
{
    threadParam_t *p=(threadParam_t*)param;
    int t_id=p->t_id;
    for(int k=0;k<n;k++)
    {
        if(t_id==0)//主线程进行除法操作
        {
            for(int j=k+1;j<n;j++)
                A[k][j]=A[k][j]/A[k][k];
            A[k][k]=1.0;
        }
        else
            sem_wait(&sem_Division[t_id-1]);//阻塞，等待完成除法操作
        
        //t_id为0的线程唤醒其他工作线程，进行消去操作
        if(t_id==0)
        {
            for(int i=0;i<worker_count-1;i++)
                sem_post(&sem_Division[i]);
        }

        //循环划分任务
        for(int i=k+1+t_id;i<n;i+=worker_count)
        {
            for(int j=k+1;j<n;j++)
                A[i][j]=A[i][j]-A[i][k]*A[k][j];
            A[i][k]=0.0;
        }
        if(t_id==0)
        {
            for(int i=0;i<worker_count-1;i++)
                sem_wait(&sem_leader);//等待其他worker完成消去
            for(int i=0;i<worker_count-1;i++)
                sem_post(&sem_Elimination[i]);//通知其他worker进入下一轮
        }
        else
        {
            sem_post(&sem_leader);//通知leader，已完成消去任务
            sem_wait(&sem_Elimination[t_id-1]);//等待通知，进入下一轮
        }
    }
    pthread_exit(NULL);
}

void pthread_static0()
{
    //初始化信号量
    sem_init(&sem_leader,0,0);
    for(int i=0;i<worker_count-1;i++)
    {
        sem_init(&sem_Division[i],0,0);
        sem_init(&sem_Elimination[i],0,0);
    }
    //创建线程
    pthread_t handles[worker_count];
    threadParam_t param[worker_count];
    for(int t_id=0;t_id<worker_count;t_id++)
    {
        param[t_id].t_id=t_id;
        pthread_create(&handles[t_id],NULL,threadFunc0,(void*)(param+t_id));
    }
    for(int t_id=0;t_id<worker_count;t_id++)
        pthread_join(handles[t_id],NULL);
    //销毁所有信号量
    sem_destroy(&sem_leader);
    for(int i=0;i<worker_count-1;i++)
    {
        sem_destroy(&sem_Division[i]);
        sem_destroy(&sem_Elimination[i]);
    }
}



void *threadFunc(void *param)
{
    threadParam_t *p=(threadParam_t*)param;
    int t_id=p->t_id;
    for(int k=0;k<n;k++)
    {
        if(t_id==0)//主线程进行除法操作
        {
            for(int j=k+1;j<n;j++)
                A[k][j]=A[k][j]/A[k][k];
            A[k][k]=1.0;
        }
        else
            sem_wait(&sem_Division[t_id-1]);//阻塞，等待完成除法操作
        
        //t_id为0的线程唤醒其他工作线程，进行消去操作
        if(t_id==0)
        {
            for(int i=0;i<worker_count-1;i++)
                sem_post(&sem_Division[i]);
        }

        // //循环划分任务
        // for(int i=k+1+t_id;i<n;i+=worker_count)
        // {
        //     for(int j=k+1;j<n;j++)
        //         A[i][j]=A[i][j]-A[i][k]*A[k][j];
        //     A[i][k]=0.0;
        // }

        //块划分任务
        int num=(n-k)/worker_count;
        int my_first=k+1+t_id*num;
        //int my_last=k+(t_id+1)*num;
        int my_last;
        if(t_id==worker_count-1)
            my_last=n;
        else
            my_last=my_first+num;
//        cout<<"t_id"<<" "<<t_id<<" "<<my_first<<" "<<my_last<<endl;
        for(int i=my_first;i<my_last;i++)
        {
            for(int j=k+1;j<n;j++)
            {
                A[i][j]=A[i][j]-A[i][k]*A[k][j];
            }
            A[i][k]=0.0;
        }


        if(t_id==0)
        {
            for(int i=0;i<worker_count-1;i++)
                sem_wait(&sem_leader);//等待其他worker完成消去
            for(int i=0;i<worker_count-1;i++)
                sem_post(&sem_Elimination[i]);//通知其他worker进入下一轮
        }
        else
        {
            sem_post(&sem_leader);//通知leader，已完成消去任务
            sem_wait(&sem_Elimination[t_id-1]);//等待通知，进入下一轮
        }
    }
    pthread_exit(NULL);
}

void pthread_static()
{
    //初始化信号量
    sem_init(&sem_leader,0,0);
    for(int i=0;i<worker_count-1;i++)
    {
        sem_init(&sem_Division[i],0,0);
        sem_init(&sem_Elimination[i],0,0);
    }
    //创建线程
    pthread_t handles[worker_count];
    threadParam_t param[worker_count];
    for(int t_id=0;t_id<worker_count;t_id++)
    {
        param[t_id].t_id=t_id;
        pthread_create(&handles[t_id],NULL,threadFunc,(void*)(param+t_id));
    }
    for(int t_id=0;t_id<worker_count;t_id++)
        pthread_join(handles[t_id],NULL);
    //销毁所有信号量
    sem_destroy(&sem_leader);
    for(int i=0;i<worker_count-1;i++)
    {
        sem_destroy(&sem_Division[i]);
        sem_destroy(&sem_Elimination[i]);
    }
}