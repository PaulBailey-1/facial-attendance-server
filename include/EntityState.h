#pragma once

#include <array>
#include <memory>

#include <boost/core/span.hpp>

#define FACE_VEC_SIZE 128

class EntityState {
public:

	int id;
	std::array<float, FACE_VEC_SIZE> facialFeatures;

	EntityState(int id_, boost::span<const UCHAR> facialFeatures_) {
		id = id_;
		memcpy(facialFeatures.data(), facialFeatures_.data(), facialFeatures_.size_bytes());
	}

	const boost::span<UCHAR> getFacialFeatures() const { return boost::span<UCHAR>(reinterpret_cast<UCHAR*>(const_cast<float*>(facialFeatures.data())), facialFeatures.size() * sizeof(float)); }

	friend bool operator < (const EntityState& a, const EntityState& b) {
		return a.id < b.id;
	}
};

class Update : public EntityState {
public:

	int deviceId;

	Update(int id_, int device_id_, boost::span<const UCHAR> facialFeatures_) : EntityState(id_, facialFeatures_) {
		deviceId = device_id_;
	}
};

class LongTermState : public EntityState {
public:
	LongTermState(int id_, boost::span<const UCHAR> facialFeatures_) : EntityState(id_, facialFeatures_) {}
};

class ShortTermState : public EntityState {

public:

	int lastUpdateDeviceId;
	int longTermStateKey;

	ShortTermState(int id_, boost::span<const UCHAR> facialFeatures_, int lastUpdateDeviceId_, int longTermStateKey_) : EntityState(id_, facialFeatures_) {
		lastUpdateDeviceId = lastUpdateDeviceId_;
		longTermStateKey = longTermStateKey_;
	}

	ShortTermState(int id_, boost::span<const UCHAR> facialFeatures_, int lastUpdateDeviceId_) : ShortTermState(id_, facialFeatures_, lastUpdateDeviceId_, -1) {}
};

typedef std::shared_ptr<EntityState> EntityStatePtr;
typedef std::shared_ptr<Update> UpdatePtr;
typedef std::shared_ptr<const Update> UpdateCPtr;
typedef std::shared_ptr<ShortTermState> ShortTermStatePtr;
typedef std::shared_ptr<const ShortTermState> ShortTermStateCPtr;
typedef std::shared_ptr<LongTermState> LongTermStatePtr;
typedef std::shared_ptr<const LongTermState> LongTermStateCPtr;