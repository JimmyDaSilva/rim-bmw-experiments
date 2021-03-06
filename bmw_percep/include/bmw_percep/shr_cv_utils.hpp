#ifndef SHR_CV_UTILS
#define SHR_CV_UTILS

#include <opencv2/opencv.hpp>
#include <pcl/point_types.h>
#include <pcl/conversions.h>
#include <pcl/point_cloud.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl/kdtree/kdtree.h>
#include <pcl/search/kdtree.h>
#include <pcl/segmentation/extract_clusters.h>
#include <pcl/common/common_headers.h>
#include <pcl/sample_consensus/sac_model_plane.h>
#include <pcl/filters/extract_indices.h>
#include <pcl/people/person_cluster.h>

/**
   Utility functions for OpenCV and PCL
**/

typedef pcl::PointXYZRGB PointT;
typedef pcl::PointCloud<PointT> PointCloudT;
typedef pcl::PointXYZ PointX;
typedef pcl::PointCloud<PointX> PointCloudX;

using namespace std;

namespace shr_cv_utils{
  //Extract only RGB, Depth, and Valid-depth maps from point cloud
  bool pc_to_img(const PointCloudT::Ptr& cloud, cv::Mat& d_rgb, 
		 cv::Mat& d_depth, cv::Mat& d_dmask);

  //Extract only RGB, Depth, and Valid-depth maps from point cloud
  bool pc_to_img(PointCloudT::ConstPtr& cloud, cv::Mat& d_rgb, 
		 cv::Mat& d_depth, cv::Mat& d_dmask);

  //Extract only Depth, and Valid-depth maps from point cloud
  bool pc_to_depth(const PointCloudX::Ptr& cloud, 
		   cv::Mat& d_depth, cv::Mat& d_dmask);

  //Convert a quaternion and translation to
  //a homogenous transformation matrix
  void to_trans_mat(const Eigen::Quaterniond quat,
		    const Eigen::Vector3d trans,
		    Eigen::Matrix4f &trans_mat);

  //Transforms PointCloud acc to the
  //homogenous transformation matrix
  //Courtesy: Kelsey Hawkins
  void transPoints(const PointCloudT::Ptr pc_in, 
		   const Eigen::Matrix4f &trans, 
		   PointCloudT::Ptr &pc_out);

  //Crops an axis-aligned cylinder from the point cloud
  void crop_axis_a_cylinder(typename PointCloudT::Ptr& cloud,
			    Eigen::Vector3f origin,
			    float radius,
			    float length);
}

#endif
