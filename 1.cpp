#include<iostream>
#include<pthread.h>
#include<sys/time.h>

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
        for(int i=k+t_id+1;i<n;i+=worker_count){
        for(int j=k+1;j<n;j++)
        {
                A[i][j]=A[i][j]-A[i][k]*A[k][j];
        }
        A[i][k]=0;}
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

int main()
{
        cin>>n;  //矩阵规模
	for(int i=0;i<n;i++)
		A = new float*[n];
        for(int i=0;i<n;i++)
		A[i] = new float[n];
        m_reset();
	//print_result();
	//cout<<endl;
	int ret=gettimeofday(&val,NULL);  
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
	ret=gettimeofday(&newval,NULL);
	 cout<<"diff:sec---"<<newval.tv_sec-val.tv_sec<<" microsec---"<<newval.tv_usec
                            -val.tv_usec<<endl;
	//print_result();
	return 0;
} 
