#include <ros/ros.h>
#include <message_filters/subscriber.h>
#include <message_filters/synchronizer.h>
#include <message_filters/sync_policies/approximate_time.h>
#include <sensor_msgs/Image.h>
#include <ros/ros.h>
#include <image_transport/image_transport.h>
#include <cv_bridge/cv_bridge.h>
#include <sensor_msgs/image_encodings.h>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <pcl/common/time.h>
#include <pcl_ros/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl_conversions/pcl_conversions.h>
#include <pcl/common/concatenate.h>
#include <pcl/common/common.h>
#include <pcl/pcl_config.h>
#include <pcl/visualization/pcl_visualizer.h> 
#include <pcl/conversions.h> 
#include <sensor_msgs/PointCloud2.h>
#include <pcl/PCLPointCloud2.h>
#include <pcl/io/pcd_io.h>
#include <pcl/people/ground_based_people_detection_app.h>
#include <pcl/sample_consensus/sac_model_plane.h>

using namespace cv;  
using namespace std; 

int bg_frames = 100; //Number of frames to build the background
std::string kinect_name = "blah";
typedef pcl::PointXYZRGB PointT;
typedef pcl::PointCloud<PointT> PointCloudT;


float 	baseline_ = 0.075;
float 	focal_length_ = 525;

unsigned width = 640; 
unsigned height = 480;

boost::mutex cloud_mutex;
cv_bridge::CvImagePtr cv_ptr;

std::string rgb_frame_id_ = "/openni_rgb_optical_frame";
std::string  depth_frame_id_ = "/openni_depth_optical_frame";

//For subtraction function
cv::Mat mask = cv::Mat::zeros(height,width,CV_32FC1);
cv::Mat frame_norm = cv::Mat::zeros(height,width,CV_32FC1);
cv::Mat bg_norm = cv::Mat::zeros(height,width,CV_32FC1);
cv::Mat mask_norm = cv::Mat::zeros(height,width,CV_32FC1);;
cv::Mat mask_thresh = cv::Mat::zeros(height,width,CV_32FC1);
cv::Mat output = cv::Mat::zeros(height,width,CV_32FC1);
cv::Mat frame_refined = cv::Mat::zeros(height,width,CV_32FC1);
cv::Mat er = cv::Mat(8,8,CV_8UC1);
cv::Mat output_norm;

float* bg_buffer;
float* frame_buffer;
float* output_buffer;
float* maskthresh_buffer;


float bad_point = numeric_limits<float>::quiet_NaN();
int learnedFrames = 0; 
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Structure used for storing color info of a point in point in pointcloud
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef union
  {
    struct
    {
      unsigned char Blue;
      unsigned char Green;
      unsigned char Red;
      unsigned char Alpha;
    };
    float float_value;
    uint32_t long_value;
  } RGBValue;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Background subtraction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

cv::Mat subtract(cv::Mat frame, cv::Mat bg ) {

float max_value = 1.0;


bg_buffer = (float*)bg.data;
frame_buffer = (float*)frame.data;
output_buffer = (float*)output.data;
maskthresh_buffer = (float*)mask_thresh.data;
 if (learnedFrames >= bg_frames) { 
   
if(learnedFrames == bg_frames){
std::cout<<"BACKGROUND DONE for " + kinect_name<<std::endl;

}
   for (unsigned j = 0; j < height*width; ++j,++bg_buffer, ++frame_buffer,++output_buffer,++maskthresh_buffer)
     {
        if ((isnan(*frame_buffer)))
          *frame_buffer = *bg_buffer;
        if(abs(*bg_buffer - *frame_buffer)>=0.15 || isnan(*bg_buffer) )
          *maskthresh_buffer = 1;
        else 

          *maskthresh_buffer=0;

     } 
       cv::erode(mask_thresh,mask_thresh,er);
        cv::multiply(frame, mask_thresh, output);                              //applying the mask
        cv::normalize(output, output_norm, 0, 1, NORM_MINMAX, CV_32FC1);
        cv::normalize(frame, frame_norm, 0, 1, NORM_MINMAX, CV_32FC1);          
        cv::normalize(bg, bg_norm, 0, 1, NORM_MINMAX, CV_32FC1);
        //cv::imshow("Subtracted Display for " + kinect_name, output_norm);
        //cv::imshow("Background for " + kinect_name, bg_norm);
        //cv::imshow("Actual Display for " + kinect_name, frame_norm);
    }
 else {
    	if(learnedFrames == 0){
       		frame.copyTo(bg);
                cv::FileStorage fs; 
                fs.open("frame.yml", cv::FileStorage::WRITE);
                fs<<"Frame"<<frame;
                fs.release();
       	 }
    	else{
       		for (unsigned j = 0; j < height*width; ++j,++bg_buffer, ++frame_buffer)
         	{	
       	 		if ( !(isnan(*frame_buffer))){
          		*bg_buffer = *frame_buffer; 
         		}     	    	
           	} 
      	  }

  
    }

    learnedFrames++; 
    return output;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Set PointT to XYZRGB/XYZRGBA. THIS WILL GIVE ERROR IF PointT1 is not XYZRGBA/XYZRGB type
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointT1> typename pcl::PointCloud<PointT1>::Ptr convertToXYZRGBPointCloud (cv::Mat rgb_in, cv::Mat depth_in) 
{
  static unsigned rgb_array_size = 0;
  static boost::shared_array<unsigned char> rgb_array ((unsigned char*)(NULL));
  unsigned char* rgb_buffer = (unsigned char*)rgb_in.data;
  float* depth_buffer = (float*)depth_in.data;
  
  boost::shared_ptr<pcl::PointCloud<PointT1> > cloud (new pcl::PointCloud<PointT1>);

  cloud->header.frame_id = rgb_frame_id_;
  cloud->height = height;
  cloud->width = width;
  cloud->is_dense = false;

  cloud->points.resize (cloud->height * cloud->width);

  //float constant = 1.0f / device_->getImageFocalLength (depth_width_);
  register float constant_x = 1.0f / focal_length_;
  register float constant_y = 1.0f / focal_length_;
  register float centerX = ((float)cloud->width - 1.f) / 2.f;
  register float centerY = ((float)cloud->height - 1.f) / 2.f;
  
  float bad_point = std::numeric_limits<float>::quiet_NaN ();
 
  // fill in XYZ values
  unsigned step = cloud->width / width;
  unsigned skip = cloud->width * step - cloud->width;
  
  int point_idx = 0;
  for (unsigned int v = 0; v < height; ++v, point_idx += skip)
  {
    for (register unsigned int u = 0; u < width; ++u, ++depth_buffer, point_idx += step)
    {
      PointT1& pt = cloud->points[point_idx];
      /// @todo Different values for these cases
      // Check for invalid measurements

      if (pcl_isfinite(*depth_buffer))
      {
        pt.z = *depth_buffer;
        pt.x = (static_cast<float> (u) - centerX) * pt.z * constant_x;
        pt.y = (static_cast<float> (v) - centerY) * pt.z * constant_y;
      }
      else
      {
        pt.x = pt.y = pt.z = bad_point;
      }
    }
  }

  // fill in the RGB values
  step = cloud->width / width;
  skip = cloud->width * step - cloud->width;
  

  point_idx = 0;
  RGBValue color;
  color.Alpha = 0;

  for (unsigned yIdx = 0; yIdx < height; ++yIdx, point_idx += skip)
  {
    for (unsigned xIdx = 0; xIdx < width; ++xIdx, point_idx += step, rgb_buffer += 3)
    {
      PointT1& pt = cloud->points[point_idx];
      
      color.Red = *rgb_buffer;
      color.Green = *(rgb_buffer + 1);
      color.Blue = *(rgb_buffer + 2);
      
      pt.rgba = color.long_value;
    }
  }
  cloud->sensor_origin_.setZero ();
  cloud->sensor_orientation_.w () = 1.0;
  cloud->sensor_orientation_.x () = 0.0;
  cloud->sensor_orientation_.y () = 0.0;
  cloud->sensor_orientation_.z () = 0.0;
  return (cloud);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void cloud_cb(const sensor_msgs::PointCloud2ConstPtr& cloud_in, PointCloudT::Ptr& cloud_out, bool* new_cloud_available_flag){
cloud_mutex.lock();

pcl::PCLPointCloud2 pcl_pc;
pcl_conversions::toPCL(*cloud_in, pcl_pc);
pcl::fromPCLPointCloud2(pcl_pc, *cloud_out);

*new_cloud_available_flag = true;
cloud_mutex.unlock();
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


void callback(const sensor_msgs::ImageConstPtr& rgb_image, const sensor_msgs::ImageConstPtr&  depth_image, cv::Mat& rgb_mat, cv::Mat& depth_mat, bool* new_cloud_available_flag){

cloud_mutex.lock();

cv_ptr = cv_bridge::toCvCopy(rgb_image);
(cv_ptr->image).copyTo(rgb_mat);

cv_ptr = cv_bridge::toCvCopy(depth_image);
(cv_ptr->image).copyTo(depth_mat);

//*cloud_out = *convertToXYZRGBPointCloud<PointT>(rgb_mat,depth_mat);

*new_cloud_available_flag = true;
cloud_mutex.unlock();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int main (int argc, char** argv)
{


ros::init(argc, argv, "listener");
ros::NodeHandle nh("~");
nh.getParam("kinect_name", kinect_name);
nh.getParam("bg_frames", bg_frames);
sensor_msgs::PointCloud2 pcd;
pcl::PCLPointCloud2 pcl_pcd;
ros::Publisher pcd_pub;


pcd_pub = nh.advertise<sensor_msgs::PointCloud2> ("/" + kinect_name + "/subtracted_pcd", 1);




Eigen::Matrix3f rgb_intrinsics_matrix;
rgb_intrinsics_matrix << 525, 0.0, 319.5, 0.0, 525, 239.5, 0.0, 0.0, 1.0; // Kinect RGB camera intrinsics
 
  //Starting the CV windows for displaying images
//cv::namedWindow("Subtracted Display for " + kinect_name);
//cv::namedWindow("Background for " + kinect_name);
//cv::namedWindow("Actual Display for " + kinect_name);
cvStartWindowThread();
PointCloudT::Ptr cloud (new PointCloudT);
cv::Mat rgb_mat = cv::Mat(height, width, CV_8UC3);
cv::Mat depth_mat = cv::Mat(height, width, CV_32FC1);
cv::Mat sub_depth_mat = cv::Mat(height, width, CV_32FC1);
cv::Mat bg = cv::Mat::zeros(height, width, CV_32FC1);
bool new_cloud_available_flag = false;

//boost::function<void (const sensor_msgs::PointCloud2ConstPtr& )> f = boost::bind (&cloud_cb, _1,  cloud, &new_cloud_available_flag);
//ros::Subscriber sub = nh.subscribe("/kinect_front/depth_registered/points", 1000, f);

message_filters::Subscriber<sensor_msgs::Image> rgb_sub(nh, "/" + kinect_name + "/rgb/image_raw", 1);
message_filters::Subscriber<sensor_msgs::Image> depth_sub(nh, "/" + kinect_name + "/depth_registered/image_raw", 1);

typedef message_filters::sync_policies::ApproximateTime<sensor_msgs::Image, sensor_msgs::Image> MySyncPolicy;
message_filters::Synchronizer<MySyncPolicy> sync(MySyncPolicy(10), rgb_sub, depth_sub);
sync.registerCallback(boost::bind(&callback, _1, _2, rgb_mat, depth_mat, &new_cloud_available_flag));


  while(!new_cloud_available_flag){ 
    boost::this_thread::sleep(boost::posix_time::milliseconds(1));
    ros::spinOnce();
    }

new_cloud_available_flag = false;



std::cout << "WAIT WHILE BACKGROUND IS BUILDING UP for " + kinect_name << std::endl;
  
 
static unsigned count = 0;
static double last = pcl::getTime ();

while(ros::ok()){
ros::spinOnce();


if (new_cloud_available_flag && cloud_mutex.try_lock ())    // if a new cloud is available
    {
      new_cloud_available_flag = false;
      sub_depth_mat =  subtract(depth_mat, bg);
      *cloud =  *convertToXYZRGBPointCloud<PointT>(rgb_mat,sub_depth_mat);
       pcl::toPCLPointCloud2(*cloud,pcl_pcd);
       pcl_pcd.header.frame_id = kinect_name + "_rgb_optical_frame";
       pcl_conversions::fromPCL(pcl_pcd, pcd);
       pcd_pub.publish(pcd);
       

	if (++count == 30)
      	{
        double now = pcl::getTime ();
        std::cout << "Average framerate for " + kinect_name + " : " << double(count)/double(now - last) << " Hz" <<  std::endl;
        count = 0;
        last = now;
       }

       cloud_mutex.unlock();
   }

}



return 0;

}