# Copyright: (C) 2017 iCub Facility - Istituto Italiano di Tecnologia
# Authors: NGUYEN Dong Hai Phuong 
# Emails: phuong.nguyen@iit.it
# CopyPolicy: Released under the terms of the GNU GPL v2.0.
#
# skeleton3D.thrift

struct Vector {
  1: list<double> content;
} (
  yarp.name = "yarp::sig::Vector"
  yarp.includefile="yarp/sig/Vector.h"
)

/**
* skeleton3D_IDL
*
* IDL Interface to \ref skeleton3D services.
*/
service skeleton3D_IDL
{
  /**
  * Sets body valence (thread).
  * @param _valence thread value for all body parts.
  * @return true/false on success/failure.
  */
  bool set_valence(1:double _valence);

  /**
  * Gets the body valence.
  * @return the current body_valence value.
  */
  double get_valence();

  /**
  * Sets the order of median filters for body parts
  * @param _order thread value for all body parts.
  * @return true/false on success/failure.
  */
  bool set_filter_order(1:i16 _order);

  /**
  * Gets the order of median filters for body parts
  * @return the current filterOrder value.
  */
  i16 get_filter_order();

  /**
  * Sets dThresholdDisparition.
  * @param _thr threshold value.
  * @return true/false on success/failure.
  */
  bool set_threshold_disparition(1:double _thr);

  /**
  * Gets the dThresholdDisparition.
  * @return the current dThresholdDisparition value.
  */
  double get_threshold_disparition();

  /**
  * Enables the function to create a fake rightHand
  * @return true/false on success/failure.
  */
  bool enable_fake_hand();

  /**
  * Disables the function to create a fake rightHand
  * @return true/false on success/failure.
  */
  bool disable_fake_hand();

  /**
  * Enables the usage of body part confidence to modulate the thread
  * @return true/false on success/failure.
  */
  bool enable_part_conf();

  /**
  * Disables the usage of body part confidence to modulate the thread
  * @return true/false on success/failure.
  */
  bool disable_part_conf();


}
