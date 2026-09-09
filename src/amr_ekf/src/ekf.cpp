#include "amr_ekf/ekf.hpp"

#include <cmath>


Ekf::Ekf()
{
  // 상태벡터 [x, y, yaw, v, omega]
  x_.setZero();

  // 초기 상태 공분산
  P_.setIdentity();
  P_ *= 0.1;

  // 프로세스 노이즈
  Q_.setZero();

  Q_(0, 0) = 0.001;  // x
  Q_(1, 1) = 0.001;  // y
  Q_(2, 2) = 0.01;   // yaw
  Q_(3, 3) = 0.05;   // v
  Q_(4, 4) = 0.05;   // omega

  // Wheel odometry 측정 노이즈
  R_wheel_.setZero();

  R_wheel_(0, 0) = 0.05;  // linear velocity
  R_wheel_(1, 1) = 0.10;  // angular velocity

  // IMU gyro 측정 노이즈
  R_imu_(0, 0) = 0.02;
}


void Ekf::predict(double dt)
{
  if (dt <= 0.0) {
    return;
  }

  const double yaw = x_(2);
  const double v = x_(3);
  const double omega = x_(4);

  //
  // 1. 상태 Prediction
  //

  x_(0) += v * std::cos(yaw) * dt;
  x_(1) += v * std::sin(yaw) * dt;
  x_(2) += omega * dt;

  //
  // yaw를 -pi ~ +pi 범위로 정규화
  //

  x_(2) = std::atan2(
    std::sin(x_(2)),
    std::cos(x_(2))
  );

  //
  // 2. Jacobian F
  //

  StateMatrix F = StateMatrix::Identity();

  F(0, 2) = -v * std::sin(yaw) * dt;
  F(0, 3) =  std::cos(yaw) * dt;

  F(1, 2) =  v * std::cos(yaw) * dt;
  F(1, 3) =  std::sin(yaw) * dt;

  F(2, 4) = dt;

  //
  // 3. Covariance Prediction
  //

  P_ = F * P_ * F.transpose() + Q_;
}
void Ekf::updateWheel(
  double linear_velocity,
  double angular_velocity)
{
  //
  // 1. 측정값 z
  //
  // wheel odom에서 측정한:
  // [v, omega]
  //

  Eigen::Matrix<double, 2, 1> z;

  z << linear_velocity,
       angular_velocity;


  //
  // 2. 측정 행렬 H
  //
  // 상태:
  // [x, y, yaw, v, omega]
  //
  // wheel odom으로 직접 측정하는 것은
  // v와 omega뿐이다.
  //

  Eigen::Matrix<double, 2, 5> H;
  H.setZero();

  H(0, 3) = 1.0;   // v
  H(1, 4) = 1.0;   // omega


  //
  // 3. Innovation
  //
  // 실제 센서 측정값 - EKF 예상 측정값
  //

  Eigen::Matrix<double, 2, 1> innovation =
    z - H * x_;


  //
  // 4. Innovation covariance
  //

  Eigen::Matrix2d S =
    H * P_ * H.transpose() + R_wheel_;


  //
  // 5. Kalman Gain
  //

  Eigen::Matrix<double, 5, 2> K =
    P_ * H.transpose() * S.inverse();


  //
  // 6. 상태 보정
  //

  x_ = x_ + K * innovation;


  //
  // 7. 공분산 보정
  //

  StateMatrix I = StateMatrix::Identity();

  P_ = (I - K * H) * P_;
}
void Ekf::updateImu(double gyro_z)
{
  //
  // 1. 측정값 z
  //
  // MPU6050에서 받은 z축 각속도
  //

  Eigen::Matrix<double, 1, 1> z;
  z(0, 0) = gyro_z;


  //
  // 2. 측정 행렬 H
  //
  // 상태:
  // [x, y, yaw, v, omega]
  //
  // IMU gyro z는 omega만 직접 측정
  //

  Eigen::Matrix<double, 1, 5> H;
  H.setZero();

  H(0, 4) = 1.0;


  //
  // 3. Innovation
  //

  Eigen::Matrix<double, 1, 1> innovation =
    z - H * x_;


  //
  // 4. Innovation covariance
  //

  Eigen::Matrix<double, 1, 1> S =
    H * P_ * H.transpose() + R_imu_;


  //
  // 5. Kalman Gain
  //

  Eigen::Matrix<double, 5, 1> K =
    P_ * H.transpose() * S.inverse();


  //
  // 6. 상태 보정
  //

  x_ = x_ + K * innovation;


  //
  // 7. 공분산 보정
  //

  StateMatrix I = StateMatrix::Identity();

  P_ = (I - K * H) * P_;
}
const Ekf::StateVector & Ekf::state() const
{
  return x_;
}