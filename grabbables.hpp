#ifndef GRABBABLES_HPP_INCLUDED
#define GRABBABLES_HPP_INCLUDED

#include "leap_object_manager.hpp"

struct grabbable
{
    objects_container* ctr = nullptr;
    bbox b;
    btRigidBody* rigid_body = nullptr;
    int parent_id = -1;
    quaternion base_diff;
    bool self_owned = false;

    bool should_hand_collide = true;
    bool does_hand_collide = true;
    float time_elapsed_since_release_ms = 3000;
    ///with kinematic objects
    float time_needed_since_release_to_recollide_ms = 1000;

    int frame_wait_restore = 0;

    btVector3 vel_back = btVector3(0,0,0);

    bool is_kinematic = false;

    float last_ftime = 0;

    vec3f avg_vel = {0,0,0};

    uint32_t last_world_id = -1;

    vec3f offset = {0,0,0};
    vec3f ideal_offset = {0,0,0};

    bool is_parented = false;
    bool slide_toward_parent = false;

    float slide_timer = 0.f;
    float slide_time_ms = 300.f;
    vec3f slide_saved_parent = {0,0,0};
    bool slide_parent_init = false;
    bool slide_towards_parent = false;
    bool can_slide = false;

    vec3f kinematic_current = {0,0,0};
    vec3f kinematic_old = {0,0,0};

    vec3f remote_pos = {0,0,0};
    quaternion remote_rot = {{0,0,0,1}};

    ///try decreasing max history, and using exponential averages etc
    ///or perhaps even a more explicit jitter removal algorithm
    ///I need to fiddle with the history, and potentially the release stuff
    ///these are the biggest impediments to juggling
    std::deque<vec3f> history;
    int max_history = 2;

    std::deque<float> angular_stability_history;
    int max_stability_history = 4;

    sf::Clock hysteresis_time;

    void reset_release_hysteresis()
    {
        hysteresis_time.restart();
    }

    bool within_release_hysteresis(float time_ms)
    {
        return hysteresis_time.getElapsedTime().asMicroseconds() / 1000.f < time_ms;
    }

    void init(objects_container* _ctr, btRigidBody* _rigid_body, bool _can_slide = false)
    {
        ctr = _ctr;
        rigid_body = _rigid_body;

        //if(ctr)
        //    b = get_bbox(ctr);
        //else
        {
            btVector3 bmin;
            btVector3 bmax;

            btTransform none;
            none.setOrigin(btVector3(0,0,0));
            none.setRotation(btQuaternion().getIdentity());

            rigid_body->getCollisionShape()->getAabb(none, bmin, bmax);

            b.min = {bmin.x(), bmin.y(), bmin.z()};
            b.max = {bmax.x(), bmax.y(), bmax.z()};
        }

        can_slide = _can_slide;

        base_diff = base_diff.identity();
    }

    void set_self_owned()
    {
        self_owned = true;
    }

    bool inside(vec3f pos, float fudge)
    {
        mat3f rot;

        //if(ctr == nullptr)
        {
            btTransform trans;

            rigid_body->getMotionState()->getWorldTransform(trans);

            btVector3 bpos = trans.getOrigin();
            btQuaternion bq = trans.getRotation();

            quaternion q = {{bq.x(), bq.y(), bq.z(), bq.w()}};

            rot = q.get_rotation_matrix();

            vec3f v = {bpos.x(), bpos.y(), bpos.z()};

            vec3f rel = pos - v;

            return within(b, rot.transp() * rel, fudge);
        }

        /*rot = ctr->rot_quat.get_rotation_matrix();

        vec3f my_pos = xyz_to_vec(ctr->pos);

        return within(b, rot.transp() * (pos - my_pos));*/
    }

    ///grabbed
    void parent(CommonRigidBodyBase* bullet_scene, int id, quaternion base, quaternion current_parent, vec3f position_offset, vec3f ideal_position_offset, bool slide = false)
    {
        parent_id = id;

        base_diff = current_parent.get_difference(base);

        should_hand_collide = false;

        make_kinematic(bullet_scene);

        mat3f hand_rot = current_parent.get_rotation_matrix();

        offset = hand_rot.transp() * position_offset;
        ideal_offset = hand_rot.transp() * ideal_position_offset;

        is_parented = true;

        if(slide && can_slide)
        {
            slide_towards_parent = true;
            slide_timer = 0;
            slide_parent_init = false;
        }

        //printf("%f %f %f %f base\n", base_diff.x(), base_diff.y(), base_diff.z(), base_diff.w());
    }

    ///released for whatever reason
    void unparent(CommonRigidBodyBase* bullet_scene, float ftime_ms)
    {
        parent_id = -1;

        should_hand_collide = true;

        time_elapsed_since_release_ms = 0;

        make_dynamic(bullet_scene, ftime_ms);

        is_parented = false;
    }

    vec3f get_pos()
    {
        btTransform newTrans;

        rigid_body->getMotionState()->getWorldTransform(newTrans);

        btVector3 bq = newTrans.getOrigin();

        return {bq.x(), bq.y(), bq.z()};
    }

    quaternion get_quat()
    {
        btTransform newTrans;

        rigid_body->getMotionState()->getWorldTransform(newTrans);

        btQuaternion bq = newTrans.getRotation();

        return {{bq.x(), bq.y(), bq.z(), bq.w()}};
    }

    void set_trans(cl_float4 clpos, quaternion m)
    {
        mat3f mat_diff = base_diff.get_rotation_matrix();

        mat3f current_hand = m.get_rotation_matrix();
        mat3f my_rot = current_hand * mat_diff;

        quaternion n;
        n.load_from_matrix(my_rot);


        vec3f absolute_pos = {clpos.x, clpos.y, clpos.z};

        ///current hand does not take into account the rotation offset when grabbing
        ///ie we'll double rotate
        vec3f offset_rot = current_hand * offset;

        vec3f pos = absolute_pos + offset_rot;

        btTransform newTrans;

        //rigid_body->getMotionState()->getWorldTransform(newTrans);

        newTrans.setOrigin(btVector3(pos.v[0], pos.v[1], pos.v[2]));
        newTrans.setRotation(btQuaternion(n.x(), n.y(), n.z(), n.w()));

        rigid_body->getMotionState()->setWorldTransform(newTrans);
        //rigid_body->setInterpolationWorldTransform(newTrans);

        //if(ctr)
        //    ctr->set_pos(conv_implicit<cl_float4>(pos));

        slide_parent_init = true;
        slide_saved_parent = absolute_pos;

        remote_pos = pos;
        remote_rot = n;

        kinematic_old = kinematic_current;
        kinematic_current = xyzf_to_vec(rigid_body->getWorldTransform().getOrigin());
    }

    void make_dynamic(CommonRigidBodyBase* bullet_scene, float ftime_ms)
    {
        if(!is_kinematic)
            return;

        float base_time = 1/90.f;
        float frame_time = ftime_ms / 1000.f;

        rigid_body->saveKinematicState(base_time);
        rigid_body->setLinearVelocity(bullet_scene->getBodyAvgVelocity(rigid_body));
        rigid_body->setAngularVelocity(bullet_scene->getBodyAvgAngularVelocity(rigid_body));

        toggleSaveMotion();

        bullet_scene->makeDynamic(rigid_body);

        is_kinematic = false;
    }

    void make_kinematic(CommonRigidBodyBase* bullet_scene)
    {
        if(is_kinematic)
            return;

        toggleSaveMotion();

        bullet_scene->makeKinematic(rigid_body);

        is_kinematic = true;
    }

    ///blender appears to calculate a kinematic velocity, but i have no idea how to get it ;_;
    void toggleSaveMotion()
    {
        frame_wait_restore = 2;
    }

    void make_no_collide_hands(CommonRigidBodyBase* bullet_scene)
    {
        if(!does_hand_collide)
            return;

        bullet_scene->changeGroup(rigid_body, collision_masks::NORMAL, collision_masks::NORMAL);

        does_hand_collide = false;
    }

    void make_collide_hands(CommonRigidBodyBase* bullet_scene)
    {
        if(does_hand_collide)
            return;

        bullet_scene->changeGroup(rigid_body, collision_masks::NORMAL, collision_masks::ALL);

        does_hand_collide = true;
    }

    ///milliseconds
    void tick(float ftime, CommonRigidBodyBase* bullet_scene)
    {
        kinematic_source s;
        s.pos = &remote_pos;
        s.rot = &remote_rot;

        bullet_scene->setKinematicSource(rigid_body, s);

        //should_hand_collide = false;

        if(time_elapsed_since_release_ms >= time_needed_since_release_to_recollide_ms && should_hand_collide)
        {
            make_collide_hands(bullet_scene);
        }

        if(!should_hand_collide)
        {
            make_no_collide_hands(bullet_scene);
        }

        time_elapsed_since_release_ms += ftime;

        //printf("%f timeelapsed\n", time_elapsed_since_release_ms);

        if(frame_wait_restore > 0)
        {
            frame_wait_restore--;

            if(frame_wait_restore == 0)
            {
                vec3f avg = {0,0,0};

                int n = 0;

                //if(history.size() > 0)
                //    history.pop_back();
                /*if(history.size() > 0)
                    history.pop_back();*/

                for(auto& i : history)
                {
                    //if(n == history.size() / 2)
                    //    break;

                    avg += i;

                    n++;
                }

                if(n > 0)
                    avg = avg / n;

                //if(n > 0)
                //    rigid_body->setLinearVelocity({avg.v[0], avg.v[1], avg.v[2]});

                //rigid_body->setLinearVelocity(bullet_scene->getBodyAvgVelocity(rigid_body));
            }
        }

        ///so the avg velocity is wrong, because it updates out of 'phase' with this function
        if(is_kinematic && last_world_id != bullet_scene->info.internal_step_id)
        {
            //rigid_body->saveKinematicState(1/60.f);

            btVector3 vel = rigid_body->getLinearVelocity();
            btVector3 angular = rigid_body->getAngularVelocity();

            //vec3f pos = get_pos();

            //printf("pos %f %f %f\n", pos.v[0], pos.v[1], pos.v[2]);

            avg_vel = avg_vel + (vec3f){vel.x(), vel.y(), vel.z()};

            avg_vel = avg_vel / 2.f;

            history.push_back({vel.x(), vel.y(), vel.z()});

            if(history.size() > max_history)
                history.pop_front();

            //angular_stability_history
            //printf("vel %f %f %f\n", vel.x(), vel.y(), vel.z());
        }

        angular_stability_history.push_back(get_scaled_angular_stability());

        if(angular_stability_history.size() > max_stability_history)
            angular_stability_history.pop_front();

        /*if(!is_kinematic)
        {
            history.clear();
        }*/

        if(is_kinematic && slide_timer < slide_time_ms && slide_towards_parent)
        {
            offset = offset * 0.95f + ideal_offset * 0.05f;
        }

        if(self_owned)
        {
            btTransform trans;
            rigid_body->getMotionState()->getWorldTransform(trans);

            vec3f pos = xyzf_to_vec(trans.getOrigin());
            quaternion qrot = convert_from_bullet_quaternion(trans.getRotation());

            ctr->set_pos(conv_implicit<cl_float4>(pos));
            ctr->set_rot_quat(qrot);
        }

        slide_timer += ftime;

        last_ftime = ftime;
        last_world_id = bullet_scene->info.internal_step_id;
    }

    float get_angular_velocity()
    {
        btVector3 vel = rigid_body->getAngularVelocity();

        vec3f ang = xyzf_to_vec(vel);

        return ang.length();
    }

    float get_scaled_angular_stability()
    {
        float maxv = 10.f;

        return clamp(1.f - (get_angular_velocity() / maxv), 0.f, 1.f);
    }

    float get_min_angular_stability()
    {
        float mins = FLT_MAX;

        if(angular_stability_history.size() == 0)
            return get_scaled_angular_stability();

        for(auto& i : angular_stability_history)
        {
            if(i < mins)
                mins = i;
        }

        return mins;
    }

    /*vec3f get_pos()
    {
        if(ctr)
        {
            return xyz_to_vec(ctr->pos);
        }
        else
        {
            btTransform trans;

            rigid_body->getMotionState()->getWorldTransform(trans);

            btVector3 pos = trans.getOrigin();
        }
    }*/

    bool is_grabbed()
    {
        return is_parented;
    }
};

struct grab_info
{
    std::map<pinch, std::vector<grabbable>> pinches_grabbed;
};

struct by_id
{
    bool operator()(const pinch& lhs, const pinch& rhs) const
    {
        return lhs.hand_id < rhs.hand_id;
    }
};

///need to disable finger -> collider collisions for x seconds after launch
///may be easier to simply disable whole collider -> hand collisions
///need to pass in leap motion object manager, give the leap object class knowledge of which fingerbone it is
///then disable collisions for the fingertips when we're above a threshold
struct grabbable_manager
{
    std::vector<grabbable*> grabbables;
    leap_motion* motion;
    CommonRigidBodyBase* bullet_scene;
    leap_object_manager* leap_object_manage;

    void init(leap_motion* leap, CommonRigidBodyBase* _bullet_scene, leap_object_manager* _leap_object_manage)
    {
        motion = leap;
        bullet_scene = _bullet_scene;
        leap_object_manage = _leap_object_manage;
    }

    grabbable* add(objects_container* ctr, btRigidBody* rigid_body, bool override_kinematic = false, bool can_slide = false)
    {
        if(rigid_body->isStaticOrKinematicObject() && !override_kinematic)
            return nullptr;

        grabbable* g = new grabbable;
        g->init(ctr, rigid_body, can_slide);

        grabbables.push_back(g);

        return g;
    }

    ///if grabbed by multiple hands -> take the average
    ///implement the above now m8
    ///milliseconds
    ///it may be that bullet is running on a fixed timestep, and we're running on an interpolated one
    ///which would mean we cant use a kinematic rigid body
    void tick(float ftime)
    {
        std::vector<pinch> pinches = motion->get_pinches();

        ///ok this needs to be hysteresis now, where we grab if > .8, but only let go if < 0.2
        float pinch_strength_to_release = 0.001f;
        float pinch_strength_to_grab = 0.6f;
        float pinch_strength_to_disable_collisions = 0.1f;
        float release_hysteresis_time_ms = 10.f;
        float pinch_radius = 10.f;

        for(pinch& p : pinches)
        {
            for(grabbable* g : grabbables)
            {
                float angular_stability = g->get_min_angular_stability();

                angular_stability /= 2.f;

                //if(g->parent_id != -1)
                //    printf("%f minstab\n", angular_stability);

                if(p.pinch_strength < pinch_strength_to_release + angular_stability)
                {
                    if(p.hand_id == g->parent_id)// && !g->within_release_hysteresis(release_hysteresis_time_ms))
                        g->unparent(bullet_scene, ftime);
                }

                //continue;
            }

            ///tips
            leap_object* index_object = leap_object_manage->get_leap_object(p.hand_id, 1, 3);
            leap_object* thumb_object = leap_object_manage->get_leap_object(p.hand_id, 0, 3);

            if(index_object != nullptr && thumb_object != nullptr)
            {
                bool in_scene = bullet_scene->bodyInScene(index_object->kinematic);
                in_scene |= bullet_scene->bodyInScene(thumb_object->kinematic);

                if(p.pinch_strength > pinch_strength_to_disable_collisions)
                {
                    if(in_scene)
                    {
                        bullet_scene->removeRigidBody(index_object->kinematic);
                        bullet_scene->removeRigidBody(thumb_object->kinematic);
                    }
                }
                else
                {
                    if(!in_scene)
                    {
                        bullet_scene->addRigidBody(index_object->kinematic);
                        bullet_scene->addRigidBody(thumb_object->kinematic);
                    }
                }
            }

            if(p.pinch_strength < pinch_strength_to_grab)
                continue;

            vec3f pinch_pos = p.pos;

            vec3f hand_pos = p.hand_pos;

            vec3f weighted = hand_pos * 0.7f + pinch_pos * 0.3f;

            ///make everything relative to hand center
            for(grabbable* g : grabbables)
            {
                bool within;

                if(g->can_slide)
                    within = g->inside(pinch_pos, pinch_radius);
                else
                    within = g->inside(pinch_pos, 0.f);

                ///slide towards parent if we're picked up by the fudge factor
                bool within_sans_fudge = g->inside(pinch_pos, 0.f);

                if(!within)
                    continue;

                if(g->parent_id != -1)
                    continue;

                /*cl_float4 gpos = g->ctr->pos;

                vec3f gfpos = {gpos.x, gpos.y, gpos.z};*/

                vec3f gfpos = g->get_pos();

                g->parent(bullet_scene, p.hand_id, g->get_quat(), p.hand_rot, gfpos - weighted, pinch_pos - weighted, !within_sans_fudge);
            }
        }

        ///for some reason the inertia works if we do this here
        ///but no in tick
        ///but if down the bottom it works in tick
        ///but not overall
        ///this works for some reason now, something funky is going on with bullet kinematics
        for(grabbable* g : grabbables)
        {
            g->tick(ftime, bullet_scene);
        }

        ///its looking increasingly likely that bullet should be sampling this
        ///rather than us forcing this on bullet
        for(grabbable* g : grabbables)
        {
            if(g->parent_id == -1)
                continue;

            bool unparent = true;

            for(pinch& p : pinches)
            {
                if(p.hand_id == g->parent_id)
                {
                    vec3f weighted = p.hand_pos * 0.7f + p.pos * 0.3f;

                    //printf("%f ang\n", g->get_angular_velocity());

                    g->set_trans(conv_implicit<cl_float4>(weighted), p.hand_rot);

                    if(p.pinch_strength >= pinch_strength_to_release)
                        g->reset_release_hysteresis();

                    unparent = false;
                }
            }

            if(unparent)
                g->unparent(bullet_scene, ftime);
        }
    }

    std::map<pinch, std::vector<btRigidBody*>, by_id> get_all_pinched(const std::vector<btRigidBody*>& check_bodies, float pinch_threshold)
    {
        std::vector<pinch> pinches = motion->get_pinches();

        std::map<pinch, std::vector<btRigidBody*>, by_id> ret;

        for(pinch& i : pinches)
        {
            float pinch_strength = i.pinch_strength;

            if(pinch_strength < pinch_threshold)
                continue;

            vec3f pos = i.pos;

            for(btRigidBody* body : check_bodies)
            {
                btCollisionShape* col = body->getCollisionShape();

                btVector3 center;
                btScalar radius;

                col->getBoundingSphere(center, radius);

                btTransform trans;

                body->getMotionState()->getWorldTransform(trans);

                center = center + trans.getOrigin();

                vec3f cen = {center.x(), center.y(), center.z()};

                if((cen - pos).length() > radius)
                    continue;

                ///we're a match! Hooray!
                ret[i].push_back(body);
            }
        }

        return ret;
    }
};


#endif // GRABBABLES_HPP_INCLUDED
