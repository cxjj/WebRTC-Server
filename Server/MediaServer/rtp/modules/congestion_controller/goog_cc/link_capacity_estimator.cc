/*
 *  Copyright 2020 The mediaserver Project Authors. All rights reserved.
 *
 *  Created on: 2020/07/16
 *      Author: max
 *		Email: Kingsleyyau@gmail.com
 *
 *  Borrow from WebRTC Project
 */

#include <rtp/modules/congestion_controller/goog_cc/link_capacity_estimator.h>

#include <algorithm>

#include <rtp/base/numerics/safe_minmax.h>

namespace mediaserver {
LinkCapacityEstimator::LinkCapacityEstimator() {
}

DataRate LinkCapacityEstimator::UpperBound() const {
	if (estimate_kbps_.has_value())
		return DataRate::kbps(
				estimate_kbps_.value() + 3 * deviation_estimate_kbps());
	return DataRate::Infinity();
}

DataRate LinkCapacityEstimator::LowerBound() const {
	if (estimate_kbps_.has_value())
		return DataRate::kbps(
				std::max(0.0,
						estimate_kbps_.value() - 3 * deviation_estimate_kbps()));
	return DataRate::Zero();
}

void LinkCapacityEstimator::Reset() {
	estimate_kbps_.reset();
}

void LinkCapacityEstimator::OnOveruseDetected(DataRate acknowledged_rate) {
	Update(acknowledged_rate, 0.05);
}

void LinkCapacityEstimator::OnProbeRate(DataRate probe_rate) {
	Update(probe_rate, 0.5);
}

void LinkCapacityEstimator::Update(DataRate capacity_sample, double alpha) {
	double sample_kbps = capacity_sample.kbps();
	if (!estimate_kbps_.has_value()) {
		estimate_kbps_ = sample_kbps;
	} else {
		estimate_kbps_ = (1 - alpha) * estimate_kbps_.value()
				+ alpha * sample_kbps;
	}
	// Estimate the variance of the link capacity estimate and normalize the
	// variance with the link capacity estimate.
	const double norm = std::max(estimate_kbps_.value(), 1.0);
	double error_kbps = estimate_kbps_.value() - sample_kbps;
	deviation_kbps_ = (1 - alpha) * deviation_kbps_
			+ alpha * error_kbps * error_kbps / norm;
	// 0.4 ~= 14 kbit/s at 500 kbit/s
	// 2.5f ~= 35 kbit/s at 500 kbit/s
	deviation_kbps_ = SafeClamp(deviation_kbps_, 0.4f, 2.5f);
}

bool LinkCapacityEstimator::has_estimate() const {
	return estimate_kbps_.has_value();
}

DataRate LinkCapacityEstimator::estimate() const {
	return DataRate::kbps(*estimate_kbps_);
}

double LinkCapacityEstimator::deviation_estimate_kbps() const {
	// Calculate the max bit rate std dev given the normalized
	// variance and the current throughput bitrate. The standard deviation will
	// only be used if estimate_kbps_ has a value.
	return sqrt(deviation_kbps_ * estimate_kbps_.value());
}
}  // namespace mediaserver
