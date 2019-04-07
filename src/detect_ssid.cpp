/** Find DARPA SubT phone network ssid
 * 
 *  Purpose: obtain a list of available wifi network ssid's
 *  search the ssid list for the phone artifact network
 *  if found, extract the network name
 * 
 * 
 * Note: program runs system command requiring sudo permission.
 * To run program without hardcoding in a password, or running the
 * program with sudo permission, the following line can be added
 * to the file /etc/sudoers. Replace username with the actual user name.
 * 
 * username ALL=(ALL) NOPASSWD:ALL
 * 
 * Build: g++ -Wall -Wextra detect_ssid.cpp 
 * 
 */

#include <ifaddrs.h>
#include <cstring>          // strerror
#include <cstdio>           // fprintf

#include <cstdlib>          // system
#include <fstream>          // ifstream
#include <sstream>          // stringstream
#include <string>
#include <vector>
#include "ros/ros.h"
#include "std_msgs/String.h"



/** 
 * @brief Reads the first interface name that starts with the letter 'w'
 * 
 * @param[out] iface_name - contains wireless interface name. Argument is not
 * changed if the ipAddress cannot be read.
 * 
 * @return 0 upon succcess, -1 upon failure
 *
 * Procedure:
 * 
 * Iterates through list of ifaddresses. Selects the first interface name
 * that begins with the letter w. The address may be either AF_INET or AF_INET6.
 * 
 * Interface may start with the character 'w' or 'e'
 * 
 * Assigns the first ifaddress to data member ipAddress that is 
 * either in the family AF_INET of AF_INET6. 
 * 
 * 
 * Assumptions:
 * 1) Interface names
 *      wireless interfaces must start with the letter 'w'
 *
 * 2) It is assumed that network devices will not have more than one active
 *    wireless interface. 
 */
int get_wireless_interface_name(std::string &iface_name)
{
    struct ifaddrs *ifaddr, *ifa;
    int family;
    
    // find the inet ifaddress
    if (getifaddrs(&ifaddr) == -1){
        fprintf(stderr, "getifaddrs failure, errno: %s\n", strerror(errno));
        return -1;
    }

    /* Walk through linked list, maintaining head pointer so we can free list later */
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL){
            continue;
        }

        family = ifa->ifa_addr->sa_family;

        /* For debugging, sisplay interface name and family (including symbolic
            form of the latter for the common families) 
        fprintf(stderr, "interface: %8s,  address family: %2d %12s\n",
                    ifa->ifa_name, family,
                    (family == AF_PACKET) ? "(AF_PACKET)" :
                    (family == AF_INET) ?   "(AF_INET)" :
                    (family == AF_INET6) ?  "(AF_INET6)" : "unknown");
        */
        
        if (family == AF_INET || family == AF_INET6) {
                     
            /* choose the first interface that starts with a w 
               and is not a loopback interface. 
               Assumes wireless interface will start with w
            */
            if(ifa->ifa_name[0] == 'w' && ifa->ifa_name[2]=='x')
            {
                fprintf(stderr, "selecting this interface: %s\n", ifa->ifa_name);
                iface_name = std::string(ifa->ifa_name);
                break;
            }
        }
    }

    freeifaddrs(ifaddr);

    if(ifa == NULL){
        fprintf(stderr, "ifa NULL, no interface selected\n");
        return -1;
    }

    return 0;
}


/**
 * @brief Searches for phone artifact network ssid
 * 
 * @param[in] ssid_filename - contains list of available network SSID's
 * @param[in] phone_artifact_ssid - target ssid to be found
 * @param[out] phone_network - contains name of phone artifact network if found.
 *                             Otherwise, empty string
 * 
 * @return true when phone_artifact_ssid is found in the file ssid_filename,
 * Otherwise, returns false.
 * 
 * Note: only searches the file for the first occurence of the phone artifact
 * ssid. If there multiple phones in the same area, then this code should
 * be modified to reflect that possibility.
 * 
 * 
 * From DARPA Subterranean Challenge forum
 * https://community.subtchallenge.com/t/cell-phone-enabled-wifi-ap/803
 * 
 * The cell phone will be running in "Hotspot" mode and thus the WIFI radio 
 * will be operating as an access point. Each cell phone artifact will broadcast 
 * its SSID over WIFI, which will be in the form of "PhoneArtifactXX" where XX 
 * will be a two-digit randomized number. The cell phone access point will employ 
 * WPS encryption and will not accept connections from team platforms
 * 
 * 
 */

bool search_for_phone_ssid(const char* ssid_filename, const char* phone_artifact_ssid, std::string& phone_network)
{
    std::size_t found;

    phone_network.clear();

    // open the file
    std::ifstream infile(ssid_filename);
    if(!infile){
        return false;
    }

    // read entire file into string
    std::string file_contents = { std::istreambuf_iterator<char>(infile), std::istreambuf_iterator<char>() };
    infile.close();

    // search for phone artifact string
    found = file_contents.find(phone_artifact_ssid);
    if(found != std::string::npos){
        
        // add 2 for XX, two digit randomized number
        phone_network = file_contents.substr(found, strlen(phone_artifact_ssid) + 2);
        return true;
    }

    return false;
    
}

/**
 * @brief scans for available network ssid's
 * 
 * @param[in] ifname - wireless device interface name
 * @param[in] ssid_filename - output filename 
 * 
 * Procedure:
 *  calls system function to scan available wireless networks.
 *  A list of network SSID's is stored in the file: ssid_filename.
 * 
 * Note: system(command)
 *  executes a command by calling /bin/sh -c command and returns
 *  after the command has been completed. During execution of the command, 
 *  SIGCHLD will be blocked, and SIGINT and SIGQUIT will be ignored.
 * 
 * Other system command options:
 *  The command: nmcli -f SSID dev wifi
 *  will often only return a single SSID, the network to which the 
 *  wireless interface is connected, and not the list of all available 
 *  wireless network connections. 
 * 
 *  Running the command: nmcli device wifi rescan 
 *  will refresh the list, but sometimes you have to wait a few seconds.
 * 
 *  The command sudo iwlist [wifi interface] scan | grep SSID 
 *  will return a list of available networks by the ESSID name.
 * 
 */
void ssid_network_scan(const char *ifname, const char* ssid_filename)
{
    std::stringstream ss;
    std::string command_string;
    std::vector<std::string> ssid_list;

    ss << "iwlist " << ifname << " scan | grep SSID > " << ssid_filename;
    command_string = ss.str();

    system(command_string.c_str()); 

}



//int main(void)
int main(int argc, char **argv)
{
    const char* ssid_filename = "ssid_list.txt";
    //const char* phone_artifact_ssid = "PhoneArtifact";
    const char* phone_artifact_ssid = "Pixel' hector";
    std::string phone_network_name;
    

    std::string wifiname;
    ros::init(argc, argv, "wifi_reader");
    ros::NodeHandle n;
    ros::Publisher chatter_pub = n.advertise<std_msgs::String>("wifiAvailable", 1000);
    ros::Rate loop_rate(20);  

    // read the local wifi interface name
    if(get_wireless_interface_name(wifiname) != 0){
        fprintf(stderr, "did not read wireless interface name, terminating\n");
        return 1;
    }
    
    while (ros::ok())
  {
	std_msgs::String msg;
	

    // scan for a list of available wifi networks
    ssid_network_scan(wifiname.c_str(), ssid_filename);

    // search the wireless network ssid list for the phone artifact network
    if( search_for_phone_ssid(ssid_filename, phone_artifact_ssid, phone_network_name) ){
        fprintf(stderr, "found %s\n", phone_network_name.c_str());
        std::stringstream ss(phone_network_name);
        msg.data = ss.str();
    }
    else{
        fprintf(stderr, "did not find %s\n", phone_artifact_ssid);
        //msg = phone_artifact_ssid;
    }
    ROS_INFO("%s", msg.data.c_str());
    chatter_pub.publish(msg);

    ros::spinOnce();

    loop_rate.sleep();
    
    
    
}
   
    return 0;
}
