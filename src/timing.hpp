#ifndef ALIA_UI_UTILITIES_TIMING_HPP
#define ALIA_UI_UTILITIES_TIMING_HPP

#include "alia.hpp"
#include <queue>

#include "nodes.hpp"

// This file provides various utilities for working with time in alia.

namespace alia {

// Currently, alia's only sense of time is that of a monotonically increasing
// millisecond counter. It's understood to have an arbitrary start point and is
// allowed to wrap around, so 'unsigned' is considered sufficient.
typedef unsigned millisecond_count;

// Request that the UI context refresh again quickly enough for smooth
// animation.
void
request_animation_refresh(dataless_context ctx);

// Get the value of the millisecond tick counter associated with the given
// UI context. This counter is updated every refresh pass, so it's consistent
// within a single frame.
// When this is called, it's assumed that something is currently animating, so
// it also requests a refresh.
millisecond_count
get_raw_animation_tick_count(dataless_context ctx);

// Same as above, but returns a signal rather than a raw integer.
static inline auto
get_animation_tick_count(dataless_context ctx)
{
    return value(get_raw_animation_tick_count(ctx));
}

// Get the number of ticks remaining until the given end time.
// If the time has passed, this returns 0.
// This ensures that the UI context refreshes until the end time is reached.
millisecond_count
get_raw_animation_ticks_left(dataless_context ctx, millisecond_count end_tick);

struct animation_timer_state
{
    bool active = false;
    millisecond_count end_tick;
};

struct animation_timer
{
    animation_timer(context ctx) : ctx_(ctx)
    {
        get_cached_data(ctx, &state_);
        update();
    }
    animation_timer(dataless_context ctx, animation_timer_state& state)
        : ctx_(ctx), state_(&state)
    {
        update();
    }
    bool
    is_active() const
    {
        return state_->active;
    }
    millisecond_count
    ticks_left() const
    {
        return ticks_left_;
    }
    void
    start(millisecond_count duration)
    {
        state_->active = true;
        state_->end_tick = get_raw_animation_tick_count(ctx_) + duration;
    }

 private:
    dataless_context ctx_;
    animation_timer_state* state_;

    void
    update()
    {
        if (state_->active)
        {
            ticks_left_ = get_raw_animation_ticks_left(ctx_, state_->end_tick);
            if (ticks_left_ == 0)
                state_->active = false;
        }
        else
        {
            ticks_left_ = 0;
        }
    }

    millisecond_count ticks_left_;
};

#if 0

// The following are interpolation curves that can be used for animations.
typedef unit_cubic_bezier animation_curve;
animation_curve static const default_curve(0.25, 0.1, 0.25, 1);
animation_curve static const linear_curve(0, 0, 1, 1);
animation_curve static const ease_in_curve(0.42, 0, 1, 1);
animation_curve static const ease_out_curve(0, 0, 0.58, 1);
animation_curve static const ease_in_out_curve(0.42, 0, 0.58, 1);

// animated_transition specifies an animated transition from one state to
// another, defined by a duration and a curve to follow.
struct animated_transition
{
    animation_curve curve;
    millisecond_count duration;

    animated_transition()
    {
    }
    animated_transition(
        animation_curve const& curve, millisecond_count duration)
        : curve(curve), duration(duration)
    {
    }
};
animated_transition static const default_transition(default_curve, 400);

#endif

#if 0

// A value_smoother is used to create smoothly changing views of values that
// actually change abruptly.
template<class Value>
struct value_smoother
{
    bool initialized = false, in_transition;
    millisecond_count duration, transition_end;
    Value old_value, new_value;
};

// value_smoother requires the ability to interpolate the values it works with.
// If the value type supplies addition and multiplication by scalers, then it
// can be interpolated using the default implementation below. Another option
// is to simply implement a compatible interpolate function directly for the
// value type.

// interpolate(a, b, factor) yields a * (1 - factor) + b * factor
template<class Value>
Value
interpolate(Value const& a, Value const& b, double factor)
{
    return a * (1 - factor) + b * factor;
}
// Overload it for floats to eliminate warnings about conversions.
static inline float
interpolate(float a, float b, double factor)
{
    return float(a * (1 - factor) + b * factor);
}
// Overload it for integers to add rounding (and eliminate warnings).
template<class Integer>
std::enable_if_t<!std::is_integral<Integer>::value, Integer>
interpolate(Integer a, Integer b, double factor)
{
    return float(a * (1 - factor) + b * factor);
}

// reset_smoothing(smoother, value) causes the smoother to transition abruptly
// to the value specified.
template<class Value>
void
reset_smoothing(value_smoother<Value>& smoother, Value const& value)
{
    smoother.in_transition = false;
    smoother.new_value = value;
    smoother.initialized = true;
}

// smooth_raw_value(ctx, smoother, x, transition) returns a smoothed view of x.
template<class Value>
Value
smooth_raw_value(
    dataless_context ctx,
    value_smoother<Value>& smoother,
    Value const& x,
    animated_transition const& transition = default_transition)
{
    if (!smoother.initialized)
        reset_smoothing(smoother, x);
    Value current_value = smoother.new_value;
    if (smoother.in_transition)
    {
        millisecond_count ticks_left
            = get_animation_ticks_left(ctx, smoother.transition_end);
        if (ticks_left > 0)
        {
            double fraction = eval_curve_at_x(
                transition.curve,
                1. - double(ticks_left) / smoother.duration,
                1. / smoother.duration);
            current_value
                = interpolate(smoother.old_value, smoother.new_value, fraction);
        }
        else
            smoother.in_transition = false;
    }
    if (is_refresh_pass(ctx) && x != smoother.new_value)
    {
        smoother.duration =
            // If we're just going back to the old value, go back in the same
            // amount of time it took to get here.
            smoother.in_transition && x == smoother.old_value
                ? (transition.duration
                   - get_animation_ticks_left(ctx, smoother.transition_end))
                : transition.duration;
        smoother.transition_end
            = get_animation_tick_count(ctx) + smoother.duration;
        smoother.old_value = current_value;
        smoother.new_value = x;
        smoother.in_transition = true;
    }
    return current_value;
}

// If you don't need direct access to the smoother, this version will manage
// it for you.
template<class Value>
Value
smooth_raw_value(
    context ctx,
    Value const& x,
    animated_transition const& transition = default_transition)
{
    value_smoother<Value>* data;
    get_cached_data(ctx, &data);
    return smooth_raw_value(ctx, *data, x, transition);
}

// smooth_value is analagous to smooth_raw_value, but it deals with accessors
// instead of raw values.

template<class Value>
optional_input_accessor<Value>
smooth_value(
    dataless_context ctx,
    value_smoother<Value>& smoother,
    accessor<Value> const& x,
    animated_transition const& transition = default_transition)
{
    optional<Value> output;
    if (is_gettable(x))
        output = smooth_raw_value(ctx, smoother, get(x), transition);
    return optional_in(output);
}

template<class Value>
optional_input_accessor<Value>
smooth_value(
    context ctx,
    accessor<Value> const& x,
    animated_transition const& transition = default_transition)
{
    value_smoother<Value>* data;
    get_cached_data(ctx, &data);
    return smooth_value(ctx, *data, x, transition);
}

#endif

} // namespace alia

#endif
