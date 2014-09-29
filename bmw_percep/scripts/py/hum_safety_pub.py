#!/usr/bin/env python

# Publishes whether it is safe for the robot to move

import roslib
roslib.load_manifest('bmw_percep')

import time
import rospy
import tf
import matplotlib.pyplot as plt
import matplotlib.axis as m_axes

from geometry_msgs.msg import PoseArray, Pose
from std_msgs.msg import Bool as BoolMsg
from math import sqrt
import numpy as np
from visualization_msgs.msg import MarkerArray, Marker
import threading

class HumanSafetyPub:
    def __init__(self):

        #plt.axis([0., 6., 0., 6.])

        #plt.axis([)
        #plt.show(1)
        self.thread_plot = threading.Thread(target=self.plot_things)


        #subscribing to -- 
        self.pers_pos_sub = rospy.Subscriber('/human/estimated/pose', 
                                             PoseArray, self.pers_cb, 
                                             queue_size=1)
        
        self.robo_listener = tf.TransformListener()

        #publishing to -- 
        # topic is latched
        self.hum_signal_pub = rospy.Publisher('/human/safety/stop', 
                                              BoolMsg, queue_size=1, latch=True)
        self.hum_signal_viz = rospy.Publisher('/human/safety/visual', 
                                              MarkerArray, queue_size=1)
        self.started_plot = False
        
        self.in_hysterisis = False
        self.hys_reset = 10.

        self.robo_frame = "base_link"
        self.robo_ee_frame = "wrist_3_link"
        self.slope = 2.5
        self.min_dist1 = 1.25;
        self.min_dist2 = 1.75;
        self.max_dist = 4.
        self.min_dist_hyster = self.min_dist2
        #initialize to don't stop human
        self.stop_human = False
        self.in_hysterisis_start = rospy.Time.now()

    #callback receives person position and velocity
    def pers_cb(self, msg):

        self.frame = msg.header.frame_id
        self.hum_time = msg.header.stamp
        self.hum_pos = np.array([msg.poses[0].position.x, msg.poses[0].position.y])
        self.hum_vel = np.array([msg.poses[1].position.x, msg.poses[1].position.y])
        self.cur_time_tf = rospy.Time(0)
        self.cur_time = rospy.Time.now()

        #Get robot transform
        try:
            self.robo_listener.waitForTransform(self.frame, self.robo_frame, 
                                                self.cur_time_tf,
                                                rospy.Duration(1./30.))
            (trans_c, rot) = self.robo_listener.lookupTransform(self.frame, 
                                                              self.robo_frame, 
                                                              self.cur_time_tf)
            (trans_ee, rot) = self.robo_listener.lookupTransform(self.frame, 
                                                              self.robo_ee_frame, 
                                                              self.cur_time_tf)
            #by default, robo-pos is center
            self.robo_pos = np.array([trans_c[0], trans_c[1]])
        except (tf.LookupException, tf.ConnectivityException, tf.ExtrapolationException):
            return

        if (not ((np.isnan(self.hum_pos)).any() or (np.isnan(self.hum_vel)).any())):
            dist_center = np.linalg.norm(np.array(self.hum_pos - [trans_c[0], trans_c[1]]))
            dist_ee = np.linalg.norm(np.array(self.hum_pos - [trans_ee[0], trans_ee[1]]))
            
            if (dist_ee<dist_center):
                trans = [trans_ee[0], trans_ee[1]]
            else:
                trans = [trans_c[0], trans_c[1]]
            
            self.robo_pos = np.array([trans[0], trans[1]])
            
            #TODO: Propagate the person's state forward in time to current time

            #distance from the robot
            person_to_rob = -self.hum_pos + self.robo_pos
            self.dist = np.linalg.norm(person_to_rob)

            #magnitude of velocity in the direction of the robot
            self.vel_mag = np.dot(self.hum_vel, person_to_rob)/self.dist

            if self.stop_human:
                
                if self.should_stop():
                    self.in_hysterisis = False

                else: #hysterisis
                    print 'Reach hysterisis'

                    if (not self.in_hysterisis):
                        print 'Set time'
                        self.in_hysterisis = True
                        self.in_hysterisis_start = self.cur_time
                        self.min_dist_hyster = self.min_dist2
                    else:
                        print 'change min-distance'
                        self.min_dist_hyster = self.min_dist2 - (self.cur_time-self.in_hysterisis_start).secs * ((self.min_dist2-self.min_dist1)/self.hys_reset)
                        print 'Time passed = ' , ((self.cur_time-self.in_hysterisis_start).secs)    
                    # print 'Cur - Hyst = ' + str((rospy.Time.now()-self.in_hysterisis_start).secs)
                    # print 'Cur -  curtime= ' + str((rospy.Time.now()-self.cur_time).secs)
                    self.stop_human = not self.can_go(self.min_dist_hyster)
            else:
                self.in_hysterisis = False
                self.stop_human = self.should_stop()

            #visualize
            if(not self.started_plot):
                self.thread_plot.start()
                self.started_plot = True
     
        else:
            #print 'No Human'
            self.stop_human = False
        
        #publish 
        stop_msg = BoolMsg()
        stop_msg.data = self.stop_human
        #print stop_msg
        self.hum_signal_pub.publish(stop_msg)
        
        # self.plot_things()
        self.pub_visuals()
        return

    def plot_things(self):
        plt.ion()
        self.fig = plt.figure(figsize=(13,13))

        plt.hold(True)

        xylims = [0., 6., -2., 2.]

        while not rospy.is_shutdown():
            if (self.in_hysterisis):
                min_dist2 = self.min_dist_hyster
            else:
                min_dist2 = self.min_dist2
            
            #plot the lines first
            line1x = [self.min_dist1, (self.min_dist1+1.)]
            line1y = [0., (self.slope*(line1x[1] - self.min_dist1))]
            line2x = [min_dist2, (min_dist2+1.)]
            line2y = [0., (self.slope * (line2x[1] - min_dist2))]
            
            fixl1x = [self.min_dist1, self.min_dist1]
            fixl1y = [0., xylims[2]]
            
            fixl2x = [min_dist2, min_dist2]
            fixl2y = [0., xylims[2]]
            
            #fix

            pointx = self.dist
            pointy = self.vel_mag
        
            plt.clf()

            plt.axis(xylims, figure=self.fig)
            plt.plot(line1x, line1y, 'r', line2x, line2y, 'b', pointx, pointy, 'g^', fixl1x, fixl1y, 'r', fixl2x, fixl2y, 'b')
            plt.draw()
            time.sleep(.03)
        #plt.close()
        

    def can_go(self, min_dist):
        if (self.dist<min_dist):
            return False
        elif (self.dist>self.max_dist):
            return True

        #velocity and distance
        evals = self.vel_mag - self.slope * (self.dist - min_dist)

        if (evals>0):
            return False
        else:
            return True
    
    def should_stop(self):

        #distance threshold
        min_dist = self.min_dist1
        if (self.dist<min_dist):
            return True
        elif (self.dist>self.max_dist):
            return False

        #velocity and distance
        evals = self.vel_mag - self.slope * (self.dist - min_dist)
        if (evals>0):
            return True
        else:
            return False

    def pub_visuals(self):
        if self.stop_human:
            color = (1.,0.,0.,1.) #r,g,b
        else:
            color = (0.,1.,0.,1.) #r,g,b
        
        viz_ = Marker()
        viz_.header.stamp = self.cur_time
        viz_.header.frame_id = self.frame
        viz_.ns = 'human/safety/visuals'
        viz_.id = 1
        viz_.type = Marker.CYLINDER
        viz_.action = Marker.ADD

        viz_.pose.position.x = self.robo_pos[0]
        viz_.pose.position.y = self.robo_pos[1]
        viz_.pose.position.z = 1.5

        viz_.pose.orientation.x = 0.
        viz_.pose.orientation.y = 0.
        viz_.pose.orientation.z = 0.
        viz_.pose.orientation.w = 1.

        viz_.scale.x = 1.
        viz_.scale.y = 1.
        viz_.scale.z = .01

        viz_.color.r = color[0]
        viz_.color.g = color[1]
        viz_.color.b = color[2]
        viz_.color.a = color[3]

        viz_.lifetime = rospy.Duration()
        
        temp_markers = MarkerArray()
        temp_markers.markers.append(viz_)
        self.hum_signal_viz.publish(temp_markers)
        
        
def main():
    rospy.init_node('human_safety_publisher')
    hum_pub = HumanSafetyPub()
    rospy.spin()

#MAIN
if __name__=='__main__':
    main()