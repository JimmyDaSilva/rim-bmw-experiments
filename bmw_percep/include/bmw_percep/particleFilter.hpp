#include<iostream>
#include<vector>
#include<opencv2/opencv.hpp>
#include<boost/random/uniform_real.hpp>
#include<boost/random/mersenne_twister.hpp>
#include<boost/random/variate_generator.hpp>
#include<numeric>
#include<boost/random/normal_distribution.hpp>
#include<limits>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

/**
   Class *definitions* for 2D particle filter
   -- has a bit of unnecessary dependence on OpenCV - to get rid of later
**/

// TODO: Have a notion of maximums and some way of keeping estimates within it
// i.e. the range in which x and y can go

using namespace std;

typedef vector<vector< double> > State;  // x, y, vx, vy and weights
typedef pcl::PointXYZRGB PointT;
typedef pcl::PointCloud<PointT> PointCloudT;

class particleFilter2D{
public:

  //deprecated constructor
  particleFilter2D();

  // initialize with two observed locations, this enables particles to
  // be initialized using both position and velocity measures
  particleFilter2D(cv::Point2f obs_1, cv::Point2f obs_2, double delta_t=(1.0/30.0), 
		   int no_part =1000, double std_acc=10.0,
		   double std_pos=1.5, double std_vel=2.0, double eff_part_ratio=0.2);
  
  //initialize particles all over again
  void reinitialize(cv::Point2f obs_1, cv::Point2f obs_2, double delta_t=(1.0/30.0), 
		   int no_part =1000, double std_acc=10.0,
		   double std_pos=1.5, double std_vel=2.0, double eff_part_ratio=0.2);
  
  //propogate particles one step in time
  void prop_part();

  //point-estimate of the state given an observation of position
  void estimate(const cv::Point2f obs, cv::Point2f &pos, cv::Point2f &vel);

  //point-estimate of the state given an observation of position & velocity
  void estimate(const cv::Point2f obs, const cv::Point2f obs_vel, 
		cv::Point2f &pos, cv::Point2f &vel);
  
  //set the image to overlay particles on
  void set_image(const typename PointCloudT::ConstPtr cloud);

  //overlay the image with particles
  void overlay_w_points();
  //set range of workspace and the leaf size
  void set_range_gran(Eigen::Vector2f r_min, Eigen::Vector2f r_max, float leaf);

private:
  void reweigh(cv::Point2f obs, double sigma=1.0); //reweigh particles
						   //according to
						   //obsevation
  void reweigh(cv::Point2f obs,   
	       cv::Point2f obs_v, 
	       double sigma=1.0,
	       double sigma_v=2.0); //reweigh particles according to
				  //obsevation of position & velocity

  void resample_particles(); //resample particles from weight
			     //distribution

  int roll_weighted_die(vector<double> disc_dist); // sample from
						   // discrete
						   // distribution

  void weighted_mean(cv::Mat& p_est); // computes weighted mean of
					// particles

  cv::Mat cur_state; // current state of particles 
  State prev_state;
  double acc_std; // standard-dev for acceleration
  int n_particles; // no. of particles
  double delta; // time-step
  double effective_part_thresh; // if the ratio of effective to
				// ineffictive particles goes below
				// threshold then resample
  cv::Point2f r_min_, r_max_; //workspace range
  double leaf_size_;
  cv::Mat overlay_im_;
  boost::mt19937 gen;
};
