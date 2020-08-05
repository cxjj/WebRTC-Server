/*
 *  Copyright 2020 The mediaserver Project Authors. All rights reserved.
 *
 *  Created on: 2020/07/16
 *      Author: max
 *		Email: Kingsleyyau@gmail.com
 *
 *  Borrow from WebRTC Project
 */

#include <rtp/modules/remote_bitrate_estimator/overuse_detector.h>

#include <math.h>
#include <stdio.h>

#include <algorithm>
#include <string>

#include <rtp/modules/remote_bitrate_estimator/bwe_defines.h>
#include <rtp/base/checks.h>
#include <rtp/base/numerics/safe_minmax.h>

namespace mediaserver {

const char kAdaptiveThresholdExperiment[] = "WebRTC-AdaptiveBweThreshold";
const char kEnabledPrefix[] = "Enabled";
const size_t kEnabledPrefixLength = sizeof(kEnabledPrefix) - 1;
const char kDisabledPrefix[] = "Disabled";
const size_t kDisabledPrefixLength = sizeof(kDisabledPrefix) - 1;

const double kMaxAdaptOffsetMs = 15.0;
const double kOverUsingTimeThreshold = 10;
const int kMaxNumDeltas = 60;

bool AdaptiveThresholdExperimentIsDisabled(
		const WebRtcKeyValueConfig& key_value_config) {
	std::string experiment_string = key_value_config.Lookup(
			kAdaptiveThresholdExperiment);
	const size_t kMinExperimentLength = kDisabledPrefixLength;
	if (experiment_string.length() < kMinExperimentLength)
		return false;
	return experiment_string.substr(0, kDisabledPrefixLength) == kDisabledPrefix;
}

// Gets thresholds from the experiment name following the format
// "WebRTC-AdaptiveBweThreshold/Enabled-0.5,0.002/".
bool ReadExperimentConstants(const WebRtcKeyValueConfig& key_value_config,
		double* k_up, double* k_down) {
	std::string experiment_string = key_value_config.Lookup(
			kAdaptiveThresholdExperiment);
	const size_t kMinExperimentLength = kEnabledPrefixLength + 3;
	if (experiment_string.length() < kMinExperimentLength
			|| experiment_string.substr(0, kEnabledPrefixLength)
					!= kEnabledPrefix)
		return false;
	return sscanf(experiment_string.substr(kEnabledPrefixLength + 1).c_str(),
			"%lf,%lf", k_up, k_down) == 2;
}

OveruseDetector::OveruseDetector(const WebRtcKeyValueConfig* key_value_config)
// Experiment is on by default, but can be disabled with finch by setting
// the field trial string to "WebRTC-AdaptiveBweThreshold/Disabled/".
:
		in_experiment_(
				!AdaptiveThresholdExperimentIsDisabled(*key_value_config)), k_up_(
				0.0087), k_down_(0.039), overusing_time_threshold_(100), threshold_(
				12.5), last_update_ms_(-1), prev_offset_(0.0), time_over_using_(
				-1), overuse_counter_(0), hypothesis_(BandwidthUsage::kBwNormal) {
	if (!AdaptiveThresholdExperimentIsDisabled(*key_value_config))
		InitializeExperiment(*key_value_config);
}

OveruseDetector::~OveruseDetector() {
}

void OveruseDetector::Reset() {
	in_experiment_ = true;
	k_up_ = 0.0087;
	k_down_ = 0.039;
//	overusing_time_threshold_ = 100;
	overusing_time_threshold_ = kOverUsingTimeThreshold;
	threshold_ = 12.5;
	last_update_ms_ = -1;
	prev_offset_ = 0.0;
	time_over_using_ = -1;
	overuse_counter_ = 0;
	hypothesis_ = BandwidthUsage::kBwNormal;
}

BandwidthUsage OveruseDetector::State() const {
	return hypothesis_;
}

BandwidthUsage OveruseDetector::Detect(double offset, double ts_delta,
		int num_of_deltas, int64_t now_ms) {
	if (num_of_deltas < 2) {
		return BandwidthUsage::kBwNormal;
	}
	const double T = std::min(num_of_deltas, kMaxNumDeltas) * offset;
//	BWE_TEST_LOGGING_PLOT(1, "T", now_ms, T);
//	BWE_TEST_LOGGING_PLOT(1, "threshold", now_ms, threshold_);
	if (T > threshold_) {
		if (time_over_using_ == -1) {
			// Initialize the timer. Assume that we've been
			// over-using half of the time since the previous
			// sample.
			time_over_using_ = ts_delta / 2;
		} else {
			// Increment timer
			time_over_using_ += ts_delta;
		}
		overuse_counter_++;
		if (time_over_using_ > overusing_time_threshold_
				&& overuse_counter_ > 1) {
			if (offset >= prev_offset_) {
				time_over_using_ = 0;
				overuse_counter_ = 0;
				hypothesis_ = BandwidthUsage::kBwOverusing;
			}
		}
	} else if (T < -threshold_) {
		time_over_using_ = -1;
		overuse_counter_ = 0;
		hypothesis_ = BandwidthUsage::kBwUnderusing;
	} else {
		time_over_using_ = -1;
		overuse_counter_ = 0;
		hypothesis_ = BandwidthUsage::kBwNormal;
	}
	prev_offset_ = offset;

	UpdateThreshold(T, now_ms);

	return hypothesis_;
}

void OveruseDetector::UpdateThreshold(double modified_offset, int64_t now_ms) {
	if (!in_experiment_)
		return;

	if (last_update_ms_ == -1)
		last_update_ms_ = now_ms;

	if (fabs(modified_offset) > threshold_ + kMaxAdaptOffsetMs) {
		// Avoid adapting the threshold to big latency spikes, caused e.g.,
		// by a sudden capacity drop.
		last_update_ms_ = now_ms;
		return;
	}

	const double k = fabs(modified_offset) < threshold_ ? k_down_ : k_up_;
	const int64_t kMaxTimeDeltaMs = 100;
	int64_t time_delta_ms = std::min(now_ms - last_update_ms_, kMaxTimeDeltaMs);
	threshold_ += k * (fabs(modified_offset) - threshold_) * time_delta_ms;
	threshold_ = SafeClamp(threshold_, 6.f, 600.f);
	last_update_ms_ = now_ms;
}

void OveruseDetector::InitializeExperiment(
		const WebRtcKeyValueConfig& key_value_config) {
	RTC_DCHECK(in_experiment_);
	double k_up = 0.0;
	double k_down = 0.0;
	overusing_time_threshold_ = kOverUsingTimeThreshold;
	if (ReadExperimentConstants(key_value_config, &k_up, &k_down)) {
		k_up_ = k_up;
		k_down_ = k_down;
	}
}
}  // namespace mediaserver
