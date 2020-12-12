#pragma once
#ifndef CATA_SRC_ACTIVITY_ACTOR_H
#define CATA_SRC_ACTIVITY_ACTOR_H

#include <memory>
#include <string>
#include <unordered_map>

#include "activity_type.h"
#include "clone_ptr.h"
#include "type_id.h"

class Character;
class JsonIn;
class JsonOut;
class player_activity;

class activity_actor
{
    private:
        /**
         * Returns true if `this` activity is resumable, and `this` and @p other
         * are "equivalent" i.e. similar enough that `this` activity
         * can be resumed instead of starting @p other.
         * Many activities are not resumable, so the default is returning
         * false.
         * @pre @p other is the same type of actor as `this`
         */
        virtual bool can_resume_with_internal( const activity_actor &,
                                               const Character & ) const {
            return false;
        }

    public:
        virtual ~activity_actor() = default;

        /**
         * Should return the activity id of the corresponding activity
         */
        virtual activity_id get_type() const = 0;

        /**
         * Called once at the start of the activity.
         * This may be used to preform setup actions and/or set
         * player_activity::moves_left/moves_total.
         */
        virtual void start( player_activity &act, Character &who ) = 0;

        /**
         * Called on every turn of the activity
         * It may be used to stop the activity prematurely by setting it to null.
         */
        virtual void do_turn( player_activity &act, Character &who ) = 0;

        /**
         * Called when the activity runs out of moves, assuming that it has not
         * already been set to null
         */
        virtual void finish( player_activity &act, Character &who ) = 0;

        /**
         * Called just before Character::cancel_activity() executes.
         * This may be used to perform cleanup
         */
        virtual void canceled( player_activity &/*act*/, Character &/*who*/ ) {}

        /**
         * Called in player_activity::can_resume_with
         * which allows suspended activities to be resumed instead of
         * starting a new activity in certain cases.
         * Checks that @p other has the same type as `this` so that
         * `can_resume_with_internal` can safely `static_cast` @p other.
         */
        bool can_resume_with( const activity_actor &other, const Character &who ) const {
            if( other.get_type() == get_type() ) {
                return can_resume_with_internal( other, who );
            }

            return false;
        }

        /**
         * Used to generate the progress display at the top of the screen
         */
        virtual std::string get_progress_message( const player_activity & ) const {
            return std::string();
        }

        /**
         * Called every turn, in player_activity::do_turn
         * (with some indirection through player_activity::exertion_level)
         * How strenuous this activity level is
         */
        virtual float exertion_level() const {
            return get_type()->exertion_level();
        }

        /**
         * Returns a deep copy of this object. Example implementation:
         * \code
         * class my_activity_actor {
         *     std::unique_ptr<activity_actor> clone() const override {
         *         return std::make_unique<my_activity_actor>( *this );
         *     }
         * };
         * \endcode
         * The returned value should behave like the original item and must have the same type.
         */
        virtual std::unique_ptr<activity_actor> clone() const = 0;

        /**
         * Must write any custom members of the derived class to json
         * Note that a static member function for deserialization must also be created and
         * added to the `activity_actor_deserializers` hashmap in activity_actor.cpp
         */
        virtual void serialize( JsonOut &jsout ) const = 0;
};

void serialize( const cata::clone_ptr<activity_actor> &actor, JsonOut &jsout );
void deserialize( cata::clone_ptr<activity_actor> &actor, JsonIn &jsin );

namespace wash
{

struct requirements {
    static constexpr float max = std::numeric_limits<float>::max();
    // Use float to allow fractional usages
    float water = 0; // charges
    float cleanser = 0; // charges
};
wash::requirements operator+( const wash::requirements &r1, const wash::requirements &r2 )
{
    return { r1.water + r2.water, r1.cleanser + r2.cleanser };
}

struct target {
    // item to wash and how many of it
    item_location loc;
    int count;
    // total requirements needed to wash this target
    requirements usage;
};

// get usable requirements( water, soap, etc.) in an inventory
wash::requirements get_available( const inventory &inv );
wash::requirements calc_total( const std::vector<wash::target> &targets );
wash::requirements round_up( const wash::requirements &reqs );
wash::requirements round_down( const wash::requirements &reqs );

}

class wash_activity_actor : public activity_actor
{
    private:
        std::vector<wash::target> targets;

        // Average number of moves required to wash an item
        // TODO: Can be improved by using volume instead, if accuracy is wanted
        //       Maybe even randomize so washing progress is in clumps
        float moves_per_item = 0;

        // For calculating elapsed moves in do_turn
        int prev_moves_left = 0;

        // For checking if we used enough moves to wash the next item
        float moves_remainder = 0;

        // Since wash requirements are floats and consumption is in integers,
        // there might by carry-overs for each successive item washing.
        // This tracks it so it can be included in the next calculations.
        requirements carryover;

    public:
        wash_activity_actor( std::vector<wash::target> targets, int total_moves_required );
        virtual activity_id get_type() const override;

        virtual void start( player_activity &act, Character &who ) override;
        virtual void do_turn( player_activity &act, Character &who ) override;
        virtual void finish( player_activity &act, Character &who ) override;
        virtual void canceled( player_activity &act, Character &who ) override;
        virtual std::string get_progress_message( const player_activity &act ) const override;

        virtual void serialize( JsonOut &jsout ) const override;
        static std::unique_ptr<activity_actor> deserialize( JsonIn &jsin );
        virtual std::unique_ptr<activity_actor> clone() const override;
};

namespace activity_actors
{

// defined in activity_actor.cpp
extern const std::unordered_map<activity_id, std::unique_ptr<activity_actor>( * )( JsonIn & )>
deserialize_functions;

} // namespace activity_actors

#endif // CATA_SRC_ACTIVITY_ACTOR_H
