#include<iostream>
#include<pthread.h>
#include<sys/time.h>
#include <semaphore.h>
#include<arm_neon.h>

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

//线程函数定义
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

//线程函数定义(neon并行化)
void *threadFunc1(void *param)
{
    threadParam_t *p=(threadParam_t*)param;
    int t_id=p->t_id;
    for(int k=0;k<n;k++)
    {
        if(t_id==0)//主线程进行除法操作
        {
            float32x4_t vt=vdupq_n_f32(A[k][k]);
            int j;
            for(j=k+1;j+4<=n;j+=4)
            {
                float32x4_t va=vld1q_f32(&A[k][j]);
                va=vdivq_f32(va,vt);
                vst1q_f32(&A[k][j],va);
            }
            for(;j<n;j++)
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
            float32x4_t vaik=vdupq_n_f32(A[i][k]);
            int j;
            for(j=k+1;j+4<=n;j+=4)
            {
                float32x4_t vakj=vld1q_f32(&A[k][j]);
				float32x4_t vaij=vld1q_f32(&A[i][j]);
				//vx ← vakj*vaik;
				float32x4_t vx=vmulq_f32(vakj,vaij);
				vaij=vsubq_f32(vaij,vx);
				//store4FloatTo(&A[i,j],vaij);
				vst1q_f32(&A[i][j],vaij);
            }
            for(;j<n;j++)
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

void pthread_static_neon()
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

int main()
{
    for(;n<2000;n+=50)
    {
    double time_2=0.0,time_2_neon=0.0;
    cin>>n;  //矩阵规模
	for(int i=0;i<n;i++)
		A = new float*[n];
    for(int i=0;i<n;i++)
		A[i] = new float[n];
    
    m_reset();
    gettimeofday(&val,NULL);
    pthread_static();
    gettimeofday(&newval,NULL);
    time_2+=(newval.tv_sec - val.tv_sec) + (double)(newval.tv_usec - val.tv_usec) / 1000000.0;
    //print_result();

    m_reset();
    gettimeofday(&val,NULL);
    pthread_static_neon();
    gettimeofday(&newval,NULL);
    time_2_neon+=(newval.tv_sec - val.tv_sec) + (double)(newval.tv_usec - val.tv_usec) / 1000000.0;

    cout<<"n:"<<n<<endl;
    cout<<"time:"<<time_2<<endl;
    cout<<"time_neon:"<<time_2_neon<<endl;
    for(int i=0;i<n;i++)
        delete A[i];
    delete A;
    }
    return 0;
}