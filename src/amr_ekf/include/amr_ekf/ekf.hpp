#pragma once

#include <Eigen/Dense>

class Ekf
{
public:
  using StateVector = Eigen::Matrix<double, 5, 1>;
  using StateMatrix = Eigen::Matrix<double, 5, 5>;

  Ekf();

  void predict(double dt);

  void updateWheel(
    double linear_velocity,
    double angular_velocity);

  void updateImu(
    double gyro_z);

  const StateVector & state() const;

private:
  // [x, y, yaw, v, omega]
  StateVector x_;

  // 상태 공분산
  StateMatrix P_;

  // 프로세스 노이즈
  StateMatrix Q_;

  // wheel 측정 노이즈: [v, omega]
  Eigen::Matrix2d R_wheel_;

  // IMU 측정 노이즈: [gyro_z]
  Eigen::Matrix<double, 1, 1> R_imu_;
};