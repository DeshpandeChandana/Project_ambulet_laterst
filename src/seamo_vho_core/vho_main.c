/*
 *  vho_main.c
 *  Choses the best network and initiates post-handoff
 *
 *  (c) SeaMo, version 0.1, 2011, ECE Department, IISc, Bangalore &
 *  Department of IT, MCIT, Government of India
 *
 *  Copyright (c) 2009 - 2011
 *  MIP Project group, ECE Department, Indian
 *  Institute of Science, Bangalore and Department of IT, Ministry of
 *  Communications and IT, Government of India. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  Authors: Seema K   <seema at eis.iisc.ernet.in>
 *           Anand SVR <anand at eis.iisc.ernet.in>
 	     Chandana S Deshpande <deshpandesanvi@gmail.com>
 *
 *  See the file "license.terms" for information on usage and redistribution
 *  of this file.
 */

/***************************** INCLUDES *****************************/
#include "vho.h"
#include "vho_wifi.h"
#include "vho_3g.h"
#include "paths.h"
#include "multi_connect.h"

/***************************** VARIABLES *****************************/
int handover;
DBusConnection *conn;
char ifname_config[16], gsm_conn_name[16];
int vrms_nw = 0;
int manswitch = 0;

int vrms_sfd;
struct sockaddr_in vrms_saddr;
socklen_t vrms_len;

int log_sfd;
struct sockaddr_in log_saddr;
socklen_t log_len;

init_vrms_connection()
{
        vrms_sfd = socket(AF_INET, SOCK_DGRAM, 0);
	if(vrms_sfd < 0){
		perror("vrms socket open failed");
	}
        bzero(&vrms_saddr, sizeof(vrms_saddr));
        vrms_saddr.sin_family = AF_INET;
        vrms_saddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	vrms_saddr.sin_port = htons(5678);

}


init_logging_network_changes()
{
        log_sfd = socket(AF_INET, SOCK_DGRAM, 0);
        if(log_sfd < 0){
                perror("vrms socket open failed");
        }
        bzero(&log_saddr, sizeof(log_saddr));
        log_saddr.sin_family = AF_INET;
        log_saddr.sin_addr.s_addr = inet_addr("127.0.0.1");
        log_saddr.sin_port = htons(8989);

}
/**************************** SUBROUTINES ******************************/
int main()
{
        int sfd, device_type, channel,num_of_3g;
        struct sockaddr_in saddr;
        socklen_t len;
	pthread_t wifi, threeg;
	char name[16];
	

        init_vrms_connection();
        init_logging_network_changes();	
#if 0
	while(1){

		send_vrms_data("wlan0", 5);
		printf("wlan0 sent\n");
		sleep(30);
		send_vrms_data("ppp1", 4);
		printf("ppp1 sent\n");
		sleep(22);
#if 0
		send_vrms_data("ppp1", 4);
		printf("wlan0 sent\n");
		sleep(30);
		send_vrms_data("ppp2", 4);
		printf("ppp2 sent\n");
		sleep(30);
#endif
		send_vrms_data("ppp2", 4);
		printf("ppp2 sent\n");
		sleep(20);
	}
#endif
	openlog("SeaMo:VHO", LOG_PID, LOG_DAEMON);

	/* Obtain a D-Bus connection handler for all D-Bus activities */
	conn = get_connection();

        init_3g_connections();
	/* socket creation for communication with vrms. (earlier this was 
  	   done in get_current_network every time it was called which is not
 	   necessary)*/
 /* Connect to the available modems */
	connect_all_modems(conn); 
	/* Check if the system is currently connected to any network */
	while (!isconnected(conn)) {
		syslog(LOG_WARNING,
		       "Please connect to any available network\n");
		sleep(1);
	}
/*
	char *active_conn[30];
	get_active_conn(conn, active_conn);

	while (1) {
		syslog(LOG_INFO, "Inside while\n");
		int conn_state = get_active_conn_state(conn, *active_conn);
		if (conn_state == 0 || conn_state == -1)	//Unknown state
			syslog(LOG_ERR, "Not Connected to any network\n");
		else if (conn_state == CONNECTED)
			break;
		else if (conn_state == ACTIVATING)
			continue;
	}
*/
	system ("sudo bash /usr/local/seamo/sbin/ip_rules_all.sh");	

	/* Create threads to receive data from pre-handoff */
	if (pthread_create(&threeg, NULL, threeg_data, NULL)) {
		syslog(LOG_ERR, "error in server threads\n");
	}
	if (pthread_create(&wifi, NULL, wifi_data, NULL)) {
		syslog(LOG_ERR, "error in server threads\n");
	}

	read_config();

	/* Get the signal when the parent process dies */
	prctl(PR_SET_PDEATHSIG, SIGHUP);


	while (1) {
		wifi_avail = wifi_param();
      	
                num_of_3g = get_num_of_modems(conn);
          		num_of_3g = 1;
                if ( num_of_3g >=1){
               	threeg_avail = threeg_param();
                }
                
                else {
                   threeg_avail == -1 ;
                   }

		printf("wifi_avail:%d num_of_3g:%d threeg_avail:%d",wifi_avail,num_of_3g,threeg_avail); 

		syslog(LOG_INFO, "wifi_avail: %d, threeg_avail: %d\n",
		       wifi_avail, threeg_avail);

		if (wifi_avail == 1 || threeg_avail == 1) {
			get_current_network(&device_type, name, &channel);

#ifdef DEBUG_L1
			syslog(LOG_INFO, "Current network: %s %d\n", name,
			       channel);
#endif

			/* If currently connected to Wi-Fi then keep the network
			 * under Observation. If the network conditions goes down
			 * then trigger the algorithm. But in case of 3G we dont 
			 * keep the network under observation, because 3G is assumed
			 * to be every where, the conditions of 3G will remain almost
			 * the same, hence we look for better Wi-Fi instead of waiting 
			 * for 3G network to degrade  */
			sleep(1);
			if (device_type == WIFI) {
				observe_wifi(name, channel);
				vho_trigger();
			} else if (device_type == GSM){
				observe_3g(name);
				vho_trigger();
			}
		}

		/* Check if parent process is alive */
		if (getppid() == 1)
			kill(getpid(), SIGHUP);
	}
	closelog();
	return 0;
}

/* This function returns the currently active network
 * @param1: buffer to store WIFI or THREEG
 * @param2: buffer to store network name
 * @param3: buffer to store operating channel
 */

void get_current_network(int *device, char *name, int *channel)
{
	int sfd;
	struct sockaddr_in saddr;
	socklen_t len;
	int active_device = get_current_network_device();

/*	if (vrms_nw == 0) {
		sfd = socket(AF_INET, SOCK_DGRAM, 0);
		bzero(&saddr, sizeof(saddr));
		saddr.sin_family = AF_INET;
		saddr.sin_addr.s_addr = inet_addr("127.0.0.1");
		saddr.sin_port = htons(5678);
		len = sizeof(saddr);

		sendto(sfd, &active_device, sizeof(active_device), 0,
		       (struct sockaddr *)&saddr, len);
		vrms_nw = 1;
	}
*/
	if (active_device == WIFI) {
		*device = WIFI;
		get_essid(name, channel);
	} else if (active_device == GSM) {
		*device = GSM;
		strcpy(name, gsm_conn_name);
	}
}

/* This function returns the essid and operating channel 
 * @param1: Buffer for essid
 * @param2: Buffer for channel
 */

int get_essid(char *name, int *channel)
{
	int sockfd;
	char essid[16];
	struct iwreq wrq;
	int ioc;

	sockfd = iw_sockets_open();
	double freq;
	struct iw_range *range;
	char buffer[sizeof(iwrange) * 2];

	memset(essid, '\0', sizeof(essid));

	/* After IOCTL call the result will be stored in iwreq
	 * structure. Hence prepare the structure to hold the
	 * results.
	 */
	wrq.u.essid.pointer = (caddr_t) essid;
	wrq.u.essid.length = IW_ESSID_MAX_SIZE + 2;
	wrq.u.essid.flags = 0;
	ioc = get_param(sockfd, SIOCGIWESSID, ifname_config, &wrq);
	strcpy(name, wrq.u.essid.pointer);	/* Obtain the essid pointed by the pointer */

	/* Prepare the structure to get wireless range info */

	bzero(buffer, sizeof(buffer));
	wrq.u.data.pointer = (caddr_t) buffer;
	wrq.u.data.length = sizeof(buffer);
	wrq.u.data.flags = 0;
	if (get_param(sockfd, SIOCGIWRANGE, ifname_config, &wrq) < 0) {
		syslog(LOG_ERR, "ERROR:%s: %s\n", ifname_config,
		       strerror(errno));
		return -1;
	}

	/* Obtain the frequency the current network is operating in */
	range = (struct iw_range *)buffer;
	ioc = get_param(sockfd, SIOCGIWFREQ, ifname_config, &wrq);
	if (ioc == -1) {
		syslog(LOG_ERR, "ERROR:%s: %s\n", ifname_config,
		       strerror(errno));
	}

	/* Convert the frequency into channel no */
	freq = iw_freq2float(&(wrq.u.freq));
	*channel = iw_freq_to_channel(freq, range);
	iw_sockets_close(sockfd);
	return 1;
}

/* This function is used invoke an ioctl call */

int get_param(int sockfd, int req, const char *ifname, struct iwreq *wrq)
{
	strncpy(wrq->ifr_name, ifname, IFNAMSIZ);
	/* Do the request */
	return (ioctl(sockfd, req, wrq));
}

/* This function will get the current active device (3G or Wi-Fi)
 * Returns the 3G or Wi-Fi */

int get_current_network_device()
{
	char *dev_list[30];
	int i = 0, state, type, num_of_dev = 0;

	/* Get the list of available interfaces for the given system.
	 * This function would give the object path of the devices */
	num_of_dev = get_device_list(conn, dev_list);

	/* For each given object path get the current state */

	for (i = 0; i < num_of_dev; i++) {
		state = get_device_state(conn, dev_list[i]);

		if (state == ACTIVE) {
			/* If the state is connected then get the type of the device
			 * corresponding to the object path */
			get_device_properties(conn, dev_list[i], &type);
			return type;
		}

	}
}

void read_config()
{
	FILE *fp;
	char conf[10][30], *line = malloc(100), *ptr;
	int i;

	/* Reading the configuration File */
	fp = fopen(CONF_DIR, "r");
	while (!feof(fp)) {
		line = fgets(line, 100, fp);

		if (line != NULL && line[0] == '#')
			continue;

		char *token = strtok(line, " ");
		i = 0;
		while (token != NULL) {
			strcpy(conf[i], token);
			token = strtok(NULL, " ");
			i++;
		}

		if (!strcmp(conf[0], "ESSID")) {
			/* Handle the newline character */
			if ((ptr = strchr(conf[IFNAME], '\n')) != NULL)
				*ptr = '\0';

			strcpy(ifname_config, conf[IFNAME]);
		} else if (!strcmp(conf[0], "CONNECTION_NAME")) {
			strcpy(gsm_conn_name, conf[APN]);
		}
	}
	fclose(fp);
}




/* When this function is triggered the QDV's of both 3G and Wi-Fi
 * are obtained. Based the QDVs the decision is made and post-handoff
 * is invoked */

void vho_trigger()
{
	float qdv_wifi, qdv_3g, qdv_diff;
	int old_dev, new_dev;
	char essid[50], apn_name[50];
        char *apn = apn_name;
	char network_name[50];
        char* ip_interface;
        int best_3g_conn;

        memset(apn_name, '\0', sizeof (apn_name));

	if (wifi_avail == 1) {
		/* Get the QDV and essid of a network which is best in Wi-Fi */
	
       printf("wifi avail = %d\n",wifi_avail);
	get_wifi_qdv(&qdv_wifi, essid);
            
	}
	if (threeg_avail == 1) {
		/* Get the QDV and APN name of 3G network */
		get_3g_qdv(&qdv_3g, apn);
            }

             if (*apn != '\0') {
                  best_3g_conn = is_connected_3g(conn,apn);
                  if(best_3g_conn == 1){
                    printf("******Best 3G network = %s is connected\n",apn);
	          } else {
                       printf ("Trying to connect to %s...\n", apn);
		      // update_modem_paths(conn,apn);
		       connect_3g_modem(conn,apn);
		       update_modem_paths(conn,apn);
	              } 	

                ip_interface  = fetch_interface(conn,apn);
#if 0
                if(ip_interface) {
                  printf("**********IP Interface for %s\t is %s********\n", apn, ip_interface);
	         }

                else{
                  printf("**********IP Interface NOT FOUND for %s********\n", apn);
                       update_modem_paths(conn,apn);
                    }
#endif 
             }

#define DEBUG

#ifdef DEBUG
	printf("QDV of WIFI: %f\t", qdv_wifi);
	printf("QDV of 3G:%f\n", qdv_3g);
#endif

	int active_device = get_current_network_device();

	if (active_device == WIFI) {

		if (((qdv_3g - qdv_wifi) >= 0) && (threeg_avail == 1)) {
			old_dev = WIFI; new_dev = GSM;
			strcpy(network_name, apn);
			qdv_diff = qdv_3g - qdv_wifi;
			printf
			    ("vho_main : Handing over from WiFi to 3G [qdv diff = %f]\n",
			     qdv_diff);
                         send_vrms_data(ip_interface, strlen(ip_interface));
//                     logging_network_change(ip_interface, strlen(ip_interface));
                     logging_network_change(apn, strlen(apn));

		}else {
			/* To handle horizontal handoff. If current essid does not match 
			 * with the essid of the network with highest QDV, then handoff
			 * to the highest QDV Wi-Fi network */

			char *name = malloc(10);
			int channel;

			/* Get current essid and channel */
			get_essid(name, &channel);

			if (!strcmp(name, essid)) {
				/* If highest QDV Wi-Fi network is current network return */

				printf
				    ("Highest QDV Wi-Fi network is current network\n");
				old_dev = new_dev = WIFI;
                                send_vrms_data("wlan0", strlen("wlan0"));
                               logging_network_change("wlan0", strlen("wlan0"));
			} else {
				strcpy(network_name, essid);
				old_dev = new_dev = WIFI;
                                send_vrms_data("wlan0", strlen("wlan0"));
                               logging_network_change("wlan0", strlen("wlan0"));
			}
			free(name);
		}
	  }else if (active_device == GSM) {

		if (active_device == GSM && ((qdv_wifi - qdv_3g) >= 0)
		    && (wifi_avail == 1)){
		
        	printf("\n Handing over from 3G to WiFi \n");
	        old_dev = GSM; new_dev = WIFI;
		strcpy(network_name, essid);
		}

               else if (active_device == GSM && ((qdv_3g - qdv_wifi) >= 0))
                    {

                if(ip_interface){
 
//                    printf("ip interface of the best conected network%s = %s \n",apn,ip_interface);
                    send_vrms_data(ip_interface, strlen(ip_interface));}                      
//		    logging_network_change(ip_interface, strlen(ip_interface));
		    logging_network_change(apn, strlen(apn));
                       return;

              }
                 else{ 
                           printf("Highest QDV GSM network is current network\n");
                          if(ip_interface) 
                           send_vrms_data(ip_interface, strlen(ip_interface));
//                     logging_network_change(ip_interface, strlen(ip_interface));
		    logging_network_change(apn, strlen(apn));

                    }
		}
  
         else {
              
                    if (threeg_avail == -1 && wifi_avail == -1) {
                 
                  printf("Both wifi and 3g interfaces not available \n");
                        return;
} 

}


        if (old_dev != new_dev) {
	/* Invoke post-handoff routine to perform the handoff task */
	printf ("[P_HO] new_dev = %d, old_dev = %d, network_name = %s\n", 
                 new_dev, old_dev, network_name);
	   post_handoff(conn, network_name, old_dev, new_dev);
	   handover = 1;
        }
}

void send_vrms_data(char *ip_interface, int msg_len){
//	int active_device = get_current_network_device();
        int iresult;

//	printf("!!!!send vrms data %s, len of send data %d\n",ip_interface,msg_len);
	iresult = sendto(vrms_sfd, ip_interface, msg_len, 0,
	       (struct sockaddr *)&vrms_saddr, sizeof(vrms_saddr));
        if (iresult < 0) perror("send_vrms_data failed");

}


void logging_network_change(char *ip_interface, int msg_len){
        int active_device = get_current_network_device();
        int iresult;

//        printf("!!!!send vrms data %s, len of send data %d\n",ip_interface,msg_len);
        iresult = sendto(log_sfd, ip_interface, msg_len, 0,
               (struct sockaddr *)&log_saddr, sizeof(log_saddr));
        if (iresult < 0) perror("logging_network_change failed");

}

