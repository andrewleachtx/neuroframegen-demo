/**
 * Add to include/dv-processing/include/dv-processing/core/frame
 * Add #include "frame/my_accumulator.hpp" to include/dv-processing/include/dv-processing/core/frame.hpp
 */
#pragma once

#include "accumulator_base.hpp"

namespace dv {

/**
 * Identical to Accumulator except allows more configuration for event contribution.
 * Each event does not necessarily have the same contribution magnitude and instead
 * can be based on a set function.
 */
class MyAccumulator : public AccumulatorBase {
public:
	/**
	 * Decay function to be used to decay the surface potential.
	 *
	 * * `NONE`: Do not decay at all. The potential can be reset manually
	 *    by calling the `clear` function
	 *
	 * * `LINEAR`: Perform a linear decay with  given slope. The linear decay goes
	 *    from currentpotential until the potential reaches the neutral potential
	 *
	 * * `EXPONENTIAL`: Exponential decay with time factor tau. The potential
	 *    eventually converges to zero.
	 *
	 * * `STEP`: Decay sharply to neutral potential after the given time.
	 *    Constant potential before.
	 */
	enum class Decay {
		NONE        = 0,
		LINEAR      = 1,
		EXPONENTIAL = 2,
		STEP        = 3
	};

private:
	// input
	bool rectifyPolarity_    = false;
	float eventContribution_ = .0;
	float maxPotential_      = .0;
	float neutralPotential_  = .0;
	float minPotential_      = .0;
	float (*weighting)(int64_t t, float x) = nullptr;
	int64_t current_start = 0;

	// decay
	Decay decayFunction_   = Decay::NONE;
	double decayParam_     = .0;
	bool synchronousDecay_ = false;

	// state
	TimeSurface decayTimeSurface_;
	cv::Mat potentialSurface_;
	int64_t highestTime_ = 0;
	int64_t lowestTime_  = -1;
	bool resetTimestamp  = true;

	// internal use methods
	/**
	 * __INTERNAL_USE_ONLY__
	 * Decays the potential at coordinates x, y to the given time, respecting the
	 * decay function. Updates the time surface to the last decay.
	 * @param x The x coordinate of the value to be decayed
	 * @param y The y coordinate of the value to be decayed
	 * @param time The time to which the value should be decayed to.
	 */
	void decay(int16_t x, int16_t y, int64_t time) {
		// normal handling for all the other functions
		int64_t lastDecayTime = decayTimeSurface_(y, x);
		dv::runtime_assert(lastDecayTime <= time, "last decay time bigger than current time, time going backwards!");

		const float lastPotential = potentialSurface_.at<float>(y, x);
		switch (decayFunction_) {
			case Decay::LINEAR: {
				potentialSurface_.at<float>(y, x)
					= (lastPotential >= neutralPotential_)
						? std::max(
							lastPotential - static_cast<float>(static_cast<double>(time - lastDecayTime) * decayParam_),
							neutralPotential_)
						: std::min(
							lastPotential + static_cast<float>(static_cast<double>(time - lastDecayTime) * decayParam_),
							neutralPotential_);
				decayTimeSurface_(y, x) = time;
				break;
			}

			case Decay::EXPONENTIAL: {
				potentialSurface_.at<float>(y, x)
					= ((lastPotential - neutralPotential_)
						  * static_cast<float>(
							  expf(-(static_cast<float>(time - lastDecayTime)) / static_cast<float>(decayParam_))))
					+ neutralPotential_;
				decayTimeSurface_(y, x) = time;
				break;
			}

			case Decay::STEP:
			// STEP decay is handled at frame generation time.
			case Decay::NONE:
			default: {
				break;
			}
		}
	}

	/**
	 * __INTERNAL_USE_ONLY__
	 * Contributes the effect of a single event onto the potential surface.
	 * @param x The x coordinate of where to contribute to
	 * @param y The y coordinate of where to contribute to
	 * @param polarity The polarity of the contribution
	 * @param weight The result of the weighting function for the current event
	 */
	void contribute(int16_t x, int16_t y, bool polarity, float contribution) {
		const float lastPotential = potentialSurface_.at<float>(y, x);
		if (!rectifyPolarity_ && !polarity) {
			contribution = -contribution;
		}

		float newPotential = std::min(std::max(lastPotential + contribution, minPotential_), maxPotential_);
		potentialSurface_.at<float>(y, x) = newPotential;
	}

public:
	/**
	 * Silly default constructor. This generates an accumulator with zero size.
	 * An accumulator with zero size does not work. This constructor just exists
	 * to make it possible to default initialize a MyAccumulator to later redefine.
	 */
	MyAccumulator() : AccumulatorBase(cv::Size(0, 0)) {
	}

	/**
	 * MyAccumulator constructor
	 * Creates a new MyAccumulator with the given params. By selecting the params
	 * the right way, the MyAccumulator can be used for a multitude of applications.
	 * The class also provides static factory functions that adjust the parameters
	 * for common use cases.
	 *
	 * @param resolution The size of the resulting frame. This must be at least the
	 * dimensions of the eventstream supposed to be added to the accumulator,
	 * otherwise this will result in memory errors.
	 * @param decayFunction The decay function to be used in this accumulator.
	 * The decay function is one of `NONE`, `LINEAR`, `EXPONENTIAL`, `STEP`. The
	 * function behave like their mathematical definitions, with LINEAR AND STEP
	 * going back to the `neutralPotential` over time, EXPONENTIAL going back to 0.
	 * @param decayParam The parameter to tune the decay function. The parameter has
	 * a different meaning depending on the decay function chosen:
	 * `NONE`: The parameter is ignored
	 * `LINEAR`: The paramaeter describes the (negative) slope of the linear function
	 * `EXPONENTIAL`: The parameter describes tau, by which the time difference is divided.
	 * @param synchronousDecay if set to true, all pixel values get decayed to the same time
	 * as soon as the frame is generated. If set to false, pixel values remain at the state
	 * they had when the last contribution came in.
	 * @param eventContribution The contribution a single event has onto the potential
	 * surface. This value gets interpreted positively or negatively depending on the
	 * event polarity
	 * @param maxPotential The upper cut-off value at which the potential surface
	 * is clipped
	 * @param neutralPotential The potential the decay function converges to over time.
	 * @param minPotential The lower cut-off value at which the potential surface
	 * is clipped
	 * @param ignorePolarity Describes if the polarity of the events should be kept
	 * or ignored. If set to true, all events behave like positive events.
	 * @param weighting The function that all events pass through to
	 * determine their contribution.
	 */
	explicit MyAccumulator(const cv::Size &resolution, MyAccumulator::Decay decayFunction = Decay::EXPONENTIAL,
		double decayParam = 1.0e+6, bool synchronousDecay = false, float eventContribution = 0.15f,
		float maxPotential = 1.0f, float neutralPotential = 0.f, float minPotential = 0.f,
		bool ignorePolarity = false, float (*weighting)(int64_t t, float x) = nullptr) :
		AccumulatorBase(resolution),
		rectifyPolarity_(ignorePolarity),
		eventContribution_(eventContribution),
		maxPotential_(maxPotential),
		neutralPotential_(neutralPotential),
		minPotential_(minPotential),
		decayFunction_(decayFunction),
		decayParam_(decayParam),
		synchronousDecay_(synchronousDecay),
		decayTimeSurface_(TimeSurface(resolution)),
		potentialSurface_(cv::Mat(resolution, CV_32F, static_cast<double>(neutralPotential))),
		highestTime_(0), 
		weighting(weighting),
		current_start(0) {
	}

	/**
	 * Accumulates all the events in the supplied packet and puts them onto the
	 * accumulation surface.
	 * @param packet The packet containing the events that should be
	 * accumulated.
	 */
	void accumulate(const EventStore &packet) override {

		if (potentialSurface_.empty()) {
			return;
		}

		if (packet.isEmpty()) {
			return;
		}

		if ((decayFunction_ == Decay::NONE) || (decayFunction_ == Decay::STEP)) {
			// for step and none, only contribute
			for (const Event &event : packet) {
				dv::runtime_assert(0 <= event.y() && event.y() <= shape_.height, "event Y coordinate out of bounds");
				dv::runtime_assert(0 <= event.x() && event.x() <= shape_.width, "event X coordinate out of bounds");

				float weight = 1;
				if (weighting != nullptr) {
					weight = weighting(event.timestamp() - current_start, eventContribution_);

				}

				contribute(event.x(), event.y(), event.polarity(), weight);
			}
		}
		else {
			// for all others, decay before contributing
			for (const Event &event : packet) {
				dv::runtime_assert(0 <= event.y() && event.y() <= shape_.height, "event Y coordinate out of bounds");
				dv::runtime_assert(0 <= event.x() && event.x() <= shape_.width, "event X coordinate out of bounds");

				float weight = 1;
				if (weighting != nullptr) {
					weight = weighting(event.timestamp() - current_start, eventContribution_);

				}

				decay(event.x(), event.y(), event.timestamp());
				contribute(event.x(), event.y(), event.polarity(), weight);
			}
		}

		if (resetTimestamp) {
			lowestTime_    = packet.getLowestTime();
			resetTimestamp = false;
		}
		highestTime_ = packet.getHighestTime();
	}

	/**
	 * Generates the accumulation frame (potential surface) at the time of the
	 * last consumed event.
	 * The function writes the output image into the given `frame` argument.
	 * The output frame will contain data with type CV_8U.
	 * @param frame the frame to copy the data to
	 */
	[[nodiscard]] dv::Frame generateFrame() override {
		cv::Mat image;

		if (synchronousDecay_ && (decayFunction_ != Decay::NONE) && (decayFunction_ != Decay::STEP)) {
			for (int y = 0; y < shape_.height; y++) {
				for (int x = 0; x < shape_.width; x++) {
					decay(static_cast<int16_t>(x), static_cast<int16_t>(y), highestTime_);
				}
			}
		}

		// Normalize min-max
		const double scaleFactor = 255.0 / static_cast<double>(maxPotential_ - minPotential_);
		const double shiftFactor = -static_cast<double>(minPotential_) * scaleFactor;
		potentialSurface_.convertTo(image, CV_8UC1, scaleFactor, shiftFactor);

		const auto frameTimestamp = lowestTime_;

		// in case of step decay function, clear potential surface
		if (decayFunction_ == Decay::STEP) {
			potentialSurface_.setTo(static_cast<double>(neutralPotential_));
			lowestTime_ = -1;
		}

		resetTimestamp = true;

		return {frameTimestamp, (highestTime_ - frameTimestamp), 0, 0, image, dv::FrameSource::ACCUMULATION};
	}

	/**
	 * Clears the potential surface by setting it to the neutral value.
	 * This function does not reset the time surface.
	 */
	void clear() {
		potentialSurface_ = cv::Mat(shape_, CV_32F, static_cast<double>(neutralPotential_));
		lowestTime_       = -1;
	}

	// setters
	/**
	 * If set to true, all events will incur a positive contribution to the
	 * potential surface
	 * @param rectifyPolarity The new value to set
	 * @deprecated Use setIgnorePolarity() method instead.
	 * @sa dv::MyAccumulator::setIgnorePolarity
	 */
	[[deprecated("Use setIgnorePolarity() method instead.")]] void setRectifyPolarity(bool rectifyPolarity) {
		setIgnorePolarity(rectifyPolarity);
	}

	/**
	 * If set to true, all events will incur a positive contribution.
	 * @param ignorePolarity The new value to set
	 */
	void setIgnorePolarity(const bool ignorePolarity) {
		MyAccumulator::rectifyPolarity_ = ignorePolarity;
	}

	/**
	 * Contribution to the potential surface an event shall incur.
	 * This contribution is either counted positively (for positive events
	 * or when `rectifyPolatity` is set).
	 * @param eventContribution The contribution a single event shall incur
	 */
	void setEventContribution(float eventContribution) {
		MyAccumulator::eventContribution_ = eventContribution;
	}

	/**
	 * @param maxPotential the max potential at which the surface should be capped at
	 */
	void setMaxPotential(float maxPotential) {
		MyAccumulator::maxPotential_ = maxPotential;
	}

	/**
	 * Set a new neutral potential value. This will also reset the cached potential surface
	 * to the given new value.
	 * @param neutralPotential The neutral potential to which the decay function should go.
	 * Exponential decay always goes to 0. The parameter is ignored there.
	 */
	void setNeutralPotential(float neutralPotential) {
		MyAccumulator::neutralPotential_ = neutralPotential;
		potentialSurface_              = cv::Mat(shape_, CV_32F, static_cast<double>(neutralPotential_));
	}

	/**
	 * @param minPotential the min potential at which the surface should be capped at
	 */
	void setMinPotential(float minPotential) {
		MyAccumulator::minPotential_ = minPotential;
	}

	/**
	 * @param decayFunction The decay function the module should use to perform the decay
	 */
	void setDecayFunction(Decay decayFunction) {
		MyAccumulator::decayFunction_ = decayFunction;
	}

	/**
	 * The decay param. This is slope for linear decay, tau for exponential decay
	 * @param decayParam The param to be used
	 */
	void setDecayParam(double decayParam) {
		MyAccumulator::decayParam_ = decayParam;
	}

	/**
	 * If set to true, all valued get decayed to the frame generation time at
	 * frame generation. If set to false, the values only get decayed on activity.
	 * @param synchronousDecay the new value for synchronoues decay
	 */
	void setSynchronousDecay(bool synchronousDecay) {
		MyAccumulator::synchronousDecay_ = synchronousDecay;
	}

	/**
	 * If not nulltpr, controls the weighting of a given event's contribution
	 * @param weighting the function to be applied to each event
	 */
	void setWeighting(float (*weighting)(int64_t t, float x)) {
		MyAccumulator::weighting = weighting;
	}

	/**
	 * If not nulltpr, controls the weighting of a given event's contribution
	 * @param weighting the function to be applied to each event
	 */
	void setCurrentStart(int64_t t) {
		MyAccumulator::current_start = t;
	}

	/**
	 * Check whether polarity rectification (ignorePolarity) is enabled.
	 * @return True if enabled, false otherwise.
	 * @deprecated Use isIgnorePolarity() method instead.
	 * @sa dv::MyAccumulator::isIgnorePolarity
	 */
	[[deprecated("Use isIgnorePolarity() method instead.")]] [[nodiscard]] bool isRectifyPolarity() const {
		return isIgnorePolarity();
	}

	/**
	 * Check whether polarity of events is ignored.
	 * @return True if polarity is ignored, false otherwise.
	 */
	[[nodiscard]] bool isIgnorePolarity() const {
		return rectifyPolarity_;
	}

	[[nodiscard]] float getEventContribution() const {
		return eventContribution_;
	}

	[[nodiscard]] float getMaxPotential() const {
		return maxPotential_;
	}

	[[nodiscard]] float getNeutralPotential() const {
		return neutralPotential_;
	}

	[[nodiscard]] float getMinPotential() const {
		return minPotential_;
	}

	[[nodiscard]] Decay getDecayFunction() const {
		return decayFunction_;
	}

	[[nodiscard]] double getDecayParam() const {
		return decayParam_;
	}

	/**
	 * Accumulates the event store into the accumulator.
	 * @param store The event store to be accumulated.
	 * @return A reference to this MyAccumulator.
	 */
	MyAccumulator &operator<<(const EventStore &store) {
		accumulate(store);
		return *this;
	}

	/**
	 * Retrieve a copy of the currently accumulated potential surface. Potential surface contains raw
	 * floating point values aggregated by the accumulator, the values are within the configured range of
	 * [minPotential; maxPotential]. This returns a deep copy of the potential surface.
	 * @return 	Potential surface image containing CV_32FC1 data.
	 */
	[[nodiscard]] cv::Mat getPotentialSurface() const {
		return potentialSurface_.clone();
	}
};

static_assert(dv::concepts::FrameOutputGenerator<MyAccumulator>);
static_assert(dv::concepts::EventToFrameConverter<MyAccumulator, dv::EventStore>);

} // namespace dv
