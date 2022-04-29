//动态线程与静态线程（信号量同步）对比
#include<iostream>
#include<pthread.h>
#include<sys/time.h>
#include <semaphore.h>
#include<arm_neon.h>
using namespace std;

const int worker_count=7;
int n;
float** A;
struct timeval val;
struct timeval newval;
typedef struct
{
        int k;
        int t_id;
}threadParam_t;
void *threadFunc(void *param)
{
        threadParam_t *p=(threadParam_t*)param;
        int k=p->k;
        int t_id=p->t_id;
        for(int i=k+t_id+1;i<n;i+=worker_count)
        {
            for(int j=k+1;j<n;j++)
            {
                    A[i][j]=A[i][j]-A[i][k]*A[k][j];
            }
            A[i][k]=0;
        }
        pthread_exit(NULL);
}



void m_reset()
{
	for(int i=0;i<n;i++){
		for(int j=0;j<i;j++)
			A[i][j]=0;
		A[i][i]=1.0;
		for(int j=i+1;j<n;j++)
			A[i][j]=rand()%100;
	}
	for(int k=0;k<n;k++)
		for(int i=k+1;i<n;i++)
			for(int j=0;j<n;j++)
				A[i][j]+=A[k][j];
}

void print_result()
{
	for(int i=0;i<n;i++)
		for(int j=0;j<n;j++)
			cout<<A[i][j]<<" ";
		cout<<endl;
}
void dynamic()
{
    for(int k=0;k<n;++k)
	{
		for(int j=k+1;j<n;j++)   
                	A[k][j]=A[k][j]/A[k][k];
        	A[k][k]=1.0;
	
		//worker_count=7;
        	pthread_t handle[worker_count];
        	threadParam_t param[worker_count];
        	//分配任务
        	for(int t_id=0;t_id<worker_count;t_id++)
        	{
                	param[t_id].k=k;
                	param[t_id].t_id=t_id;
        	}
        	//创建线程
         	for(int t_id=0;t_id<worker_count;t_id++)
                	pthread_create(&handle[t_id],NULL,threadFunc,(void*)(param+t_id));		
        	//主线程挂起等待所有的工作线程完成此轮消去工作
		for(int t_id=0;t_id<worker_count;t_id++)
			pthread_join(handle[t_id],NULL);
	}
}

//线性数据结构定义
typedef struct
{
    int t_id;
}threadParam_t1;

//信号量定义
sem_t sem_leader;
sem_t sem_Division[worker_count-1];
sem_t sem_Elimination[worker_count-1];

//线程函数定义
void *threadFunc1(void *param)
{
    threadParam_t1 *p=(threadParam_t1*)param;
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
    threadParam_t1 param[worker_count];
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

//串行
void normal()
{
    for(int k=0;k<n;k++)
	{
		for(int j=k+1;j<n;j++)
			A[k][j]=A[k][j]/A[k][k];
		A[k][k]=1.0;
		for(int i=k+1;i<n;i++)
		{
			for(int j=k+1;j<n;j++)
				A[i][j]=A[i][j]-A[i][k]*A[k][j];
			A[i][k]=0;
		}
	}
}

int main()
{
    //cin>>n;  //矩阵规模
    int step=10;
    for(;n<=2000;n+=step)
    {
	for(int i=0;i<n;i++)
		A = new float*[n];
    for(int i=0;i<n;i++)
		A[i] = new float[n];
    double time_0=0.0,time_1=0.0,time_2=0.0,time_3=0.0;


    m_reset();
    gettimeofday(&val,NULL);
    for(int i=0;i<10;i++)
        normal();
    gettimeofday(&newval,NULL);
    time_0+=(newval.tv_sec - val.tv_sec) + (double)(newval.tv_usec - val.tv_usec) / 1000000.0; 
	
    m_reset();
    gettimeofday(&val,NULL);
    for(int i=0;i<10;i++)
        dynamic();
    gettimeofday(&newval,NULL);
    time_1+=(newval.tv_sec - val.tv_sec) + (double)(newval.tv_usec - val.tv_usec) / 1000000.0; 
	
    m_reset();
    gettimeofday(&val,NULL);
    for(int i=0;i<10;i++)
        pthread_static();
    gettimeofday(&newval,NULL);
    time_2+=(newval.tv_sec - val.tv_sec) + (double)(newval.tv_usec - val.tv_usec) / 1000000.0; 
    cout<<"        "<<n<<"        "<<"& "<<time_0<<"   "<<"& "<<time_1<<"   "<<"& "<<time_2<<" "<<R"(\\ \hline)"<<endl;
    for(int i=0;i<n;i++)
        delete A[i];
    delete A;
    if(n==100){step=100;}
    if(n==1000){step=1000;}

    }
    return 0;
} 