#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <mosquitto.h>
#include <errno.h>

#define HOST "47.108.223.15"
#define PORT 1883
#define KEEP_ALIVE 60
#define MSG_MAX_SIZE 512


static int running = 1;


void my_connect_callback(struct mosquitto *mosq,void *obj,int rc)
{
    printf("Call the function: my_connect_callback.\n");
}

void my_disconnect_callback(struct mosquitto *mosq,void *obj,int rc)
{
    printf("Call the function: my_disconnect_callback.\n");
    running = 0;
}

void my_publish_callback(struct mosquitto *mosq,void *obj, int mid){
    //printf("published\n");
}

void my_message_callback(struct mosquitto *mosq,void *obj, const struct mosquitto_message *msg){
    //printf("receviced\n");
    if(msg->topic[3]=='r'){
        int fd = open("/sys/class/leds/red/brightness",O_WRONLY);
        if(fd<0){
            printf("open erro\n");
            return;
        }
        //printf("value:%s\n",msg->payload);
        write(fd,msg->payload,3);
        close(fd);
    }
    else if(msg->topic[3]=='g')
        printf("topic wrong\n");
}

int main (int argc, char **argv)
{

    int rv;
    struct mosquitto *mosq;

    //初始化库（必须）
    mosquitto_lib_init();

    mosq = mosquitto_new("pub_test",true,NULL);
    if(mosq == NULL)
    {
        printf("New pub_test error: %s\n",strerror(errno));
        mosquitto_lib_cleanup();
        return -1;
    }

    //设置回调函数
    mosquitto_connect_callback_set(mosq,my_connect_callback);
    mosquitto_disconnect_callback_set(mosq,my_disconnect_callback);
    mosquitto_message_callback_set(mosq,my_message_callback);
    mosquitto_publish_callback_set(mosq,my_publish_callback);

    //用户密码，连接
    mosquitto_username_pw_set(mosq,"qrs","123456");
    rv = mosquitto_connect(mosq,HOST,PORT,KEEP_ALIVE);

    if(rv)
    {
        printf("Connect server error: %s\n",strerror(errno));
        mosquitto_destroy(mosq);
        mosquitto_lib_cleanup();
        return -1;
    }

    //打开文件
    int dht11fd = open("/dev/dht11",O_RDONLY);              
    int ap3216cfd = open("/dev/I2C_AP3216C",O_RDONLY);
    int mpu6050fd = open("/dev/I2C1_mpu6050",O_RDONLY);  
    if(dht11fd<0 || ap3216cfd<0 || mpu6050fd<0){
        perror("open error");
        return -1;
    }

    //定义数组来读取传感器，和发送消息
    char sendData[50];          //publish所用data数组，必须为char,否则接收为乱码，且长度一定要足够
    char data8[5];              //8位数据
    short data16[6];            //16位数据

    printf("Start!\n");
    
    //订阅消息
    mosquitto_subscribe(mosq,NULL,"ledr",0);
    mosquitto_subscribe(mosq,NULL,"ledg",0);
    //开启线程
    mosquitto_loop_start(mosq);

    
    while(1){
        
        //读取发送dht11
        read(dht11fd,data8,5);
        sprintf(sendData,"%d.%d/%d.%d",data8[2],data8[3],data8[0],data8[1]);
        mosquitto_publish(mosq,NULL,"dht11",strlen(sendData),sendData,0,0);

        //读取发送ap3216c(i2c1)
        read(ap3216cfd,data16,6);
        sprintf(sendData,"%d/%d/%d",data16[0],data16[1],data16[2]);
        mosquitto_publish(mosq,NULL,"ap3216c",strlen(sendData),sendData,0,0);
 
        //读取发送mpu6050(i2c1)
        read(mpu6050fd,data16,12);
        sprintf(sendData,"%d/%d/%d/%d/%d/%d",data16[0],data16[1],data16[2],data16[3],data16[4],data16[5]);
        mosquitto_publish(mosq,NULL,"mpu6050",strlen(sendData),sendData,0,0);

        //延时循环发送
        usleep(2000*1000);
    }

    mosquitto_disconnect(mosq);
    mosquitto_loop_stop(mosq,false);
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
    printf("End!\n");

    return 0;
} 


