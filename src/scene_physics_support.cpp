#include "scene_physics_support.h"

std::string getObjectIDFromCollisionObject(const btCollisionObject* object)
{
    if (object->getUserPointer() != NULL)
        return *(std::string*)object->getUserPointer();
    else
        return std::string("unrecognized_object");
}

void assignAllConnectedToParentVertices(SceneSupportGraph &input_graph, const vertex_t &parent_vertex)
{
    boost::graph_traits<SceneSupportGraph>::out_edge_iterator out_i, out_end;
    for (tie(out_i, out_end) = boost::out_edges(parent_vertex, input_graph); out_i != out_end; ++out_i) 
    {
        vertex_t connected_vertex = boost::target(*out_i, input_graph);
        if (parent_vertex == connected_vertex) return;
        else
        {
            scene_support_vertex_properties &support_property = input_graph[connected_vertex];
            support_property.ground_supported_ = true;

            std::cerr << support_property.object_id_ << " support contribution = " 
                << support_property.support_contributions_
                << " total penetration depth = " << support_property.penetration_distance_
                << " total penetration volume = " << support_property.colliding_volume_
                << std::endl;
            // recursively call the connected vertex to assign the leaf of vertex connected to the parent

            // @TODO Check whether I need to recurvisely do this or not here
            assignAllConnectedToParentVertices(input_graph, connected_vertex);
        }
    }
}

btAABB getCollisionAABB(const btCollisionObject* obj, const btManifoldPoint &pt, const bool &is_body_0, int &shape_index)
{
    btAABB shape_AABB;
    const btCollisionShape* col_shape = obj->getCollisionShape();
    btTransform obj_location = obj->getWorldTransform();
    if (col_shape->isCompound())
    {
        const btCompoundShape* compound_shape = (btCompoundShape*)col_shape;
        shape_index = is_body_0 ? pt.m_index0 : pt.m_index1;
        const btCollisionShape* shape = compound_shape->getChildShape (shape_index);
        btTransform shape_location = compound_shape->getChildTransform (shape_index);
        shape->getAabb(obj_location * shape_location, shape_AABB.m_min, shape_AABB.m_max);
    }
    else
    {
        shape_index = 0;
        col_shape->getAabb(obj_location, shape_AABB.m_min, shape_AABB.m_max);
    }
    return shape_AABB;
}

double getIntersectingVolume(const btAABB &shapeAABB_a, const btAABB &shapeAABB_b)
{
    btAABB intersecting_box;
    shapeAABB_a.find_intersection(shapeAABB_b,intersecting_box);
    btVector3 &m_min = intersecting_box.m_min, &m_max = intersecting_box.m_max;
    double volume = 1;
    for (int i = 0; i < 3 ; i++) volume *= m_max[i] - m_min[i];
    return std::abs(volume);
}

SceneSupportGraph generateObjectSupportGraph(btDynamicsWorld *world, 
    std::map<std::string, vertex_t> &vertex_map, 
    const btScalar &time_step, const btVector3 &gravity, const bool &debug_mode)
{
    if (debug_mode) std::cerr << "Creating support graph\n";
    // make the graph with number of vertices = number of collision objects
	SceneSupportGraph scene_support_graph(world->getNumCollisionObjects());
    vertex_map.clear();

    double dTime_times_gravity = time_step * GRAVITY_MAGNITUDE * SCALING;

    for (std::size_t i = 0; i < world->getNumCollisionObjects(); i++)
    {
        scene_support_vertex_properties new_vertex_property;
        new_vertex_property.collision_object_ = world->getCollisionObjectArray()[i];
        new_vertex_property.object_id_ = getObjectIDFromCollisionObject(new_vertex_property.collision_object_);
        new_vertex_property.object_pose_ = new_vertex_property.collision_object_->getWorldTransform();
        vertex_t new_vertex = boost::add_vertex(scene_support_graph);
        scene_support_graph[new_vertex] = new_vertex_property;
        vertex_map[new_vertex_property.object_id_] = new_vertex;
    }

    if (debug_mode) std::cerr << "Checking collisions and adding edges\n";
	int numManifolds = world->getDispatcher()->getNumManifolds();
    for (int i = 0; i < numManifolds; i++)
    {
        btPersistentManifold* contactManifold =  world->getDispatcher()->getManifoldByIndexInternal(i);
        const btCollisionObject* obj_a = contactManifold->getBody0();
        const btCollisionObject* obj_b = contactManifold->getBody1();

        // all objects should be a btRigidBody
        if (obj_a->getInternalType() == 2 && obj_b->getInternalType() == 2)
        {
            btAABB shapeAABB_a, shapeAABB_b;

            btRigidBody *lower_obj, *upper_obj;
            int shape_index_a, shape_index_b;
            int shape_index_lower, shape_index_upper;
            btScalar obj_b_normal_sum = 0;

            btScalar totalImpact = 0.;
            btScalar total_collision_penetration = 0;
            btScalar total_volume_penetration = 0;

            // if (debug_mode) std::cerr << "Inspecting collision points\n";
            for (int p = 0; p < contactManifold->getNumContacts(); p++)
            {
                btManifoldPoint& pt = contactManifold->getContactPoint(p);
                totalImpact += pt.m_appliedImpulse;

                // add penetration depth if it is in collision
                if (pt.getDistance() < 0)
                {
                    total_collision_penetration += pt.getDistance() < 0 ? -pt.getDistance() : 0;

                    // measure scaled normal forces to determine whether the object is supporting/supported object
                    obj_b_normal_sum += -pt.getDistance() * pt.m_appliedImpulse * pt.m_normalWorldOnB.dot(gravity)/SCALING;
                    
                    // get intersecting AABB
                    shapeAABB_a = getCollisionAABB(obj_a, pt, true, shape_index_a);
                    shapeAABB_b = getCollisionAABB(obj_b, pt, false, shape_index_b);

                    total_volume_penetration += getIntersectingVolume(shapeAABB_a,shapeAABB_b);
                }
            }

            if (obj_b_normal_sum == 0) {continue;} // no actual collision happened
            else if (obj_b_normal_sum < 0)
            {
                // object that are supporting the other object will have its normal forces direction in
                // opposite direction of gravity vector
                lower_obj = (btRigidBody*)obj_b;
                upper_obj = (btRigidBody*)obj_a;
                shape_index_lower = shape_index_b;
                shape_index_upper = shape_index_a;
            }
            else
            {
                lower_obj = (btRigidBody*)obj_a;
                upper_obj = (btRigidBody*)obj_b;
                shape_index_lower = shape_index_a;
                shape_index_upper = shape_index_b;
            }

            // assign the graph direction to the supporting object
            vertex_t object_u,supported_object;
            object_u = vertex_map[getObjectIDFromCollisionObject(lower_obj)];
            supported_object = vertex_map[getObjectIDFromCollisionObject(upper_obj)];
            
            // if (debug_mode) std::cerr << "Inspecting edges\n";
            // check whether the edge is already exist or not
            edge_t detected_edge; bool edge_already_exist = false, add_edge_success = false;
            edge_t edge_to_update;
            boost::tie(detected_edge, edge_already_exist) = boost::edge(object_u,supported_object,scene_support_graph);

            if (total_collision_penetration > 0 && totalImpact > 0)
            {
                scene_support_graph[object_u].penetration_distance_ += total_collision_penetration;

                scene_support_graph[supported_object].penetration_distance_ += total_collision_penetration;

                // update the support contribution based on
                // the supporting forces relative to the supported object mass
                // impulse: kg m/s; support_contribution = impulse/(dT * mass * gravity)
                scene_support_graph[object_u].support_contributions_ += totalImpact*upper_obj->getInvMass()/dTime_times_gravity;

                // std::cerr << "Total impact " << getObjectIDFromCollisionObject(lower_obj) << ", " 
                //     << getObjectIDFromCollisionObject(upper_obj) << "= " << totalImpact << ", penetration= " 
                //     << total_collision_penetration << std::endl;
                if (debug_mode) {
                    std::cerr << "Inspecting edge between " << getObjectIDFromCollisionObject(lower_obj) << " and " 
                        << getObjectIDFromCollisionObject(upper_obj) << ": ";
                    std::cerr << "Total penetration volume: " << total_volume_penetration << std::endl;
                    // std::cerr << "obj_b_normal_sum: " << obj_b_normal_sum << std::endl
                }

                if (edge_already_exist) { edge_to_update = detected_edge; }
                else
                {
                    edge_t new_edge;

                    // make sure it actually really does not exist
                    boost::tie(detected_edge, edge_already_exist) = boost::edge(supported_object,object_u,scene_support_graph);
                    if (edge_already_exist)
                    {
                        edge_to_update = detected_edge;
                        if (debug_mode) std::cerr << "Something is wrong, this edge already exists! Skipped\n";
                    }
                    else
                    {
                        boost::tie(new_edge,add_edge_success) = boost::add_edge(object_u,supported_object,scene_support_graph);
                        if (!add_edge_success)
                        {
                            std::cerr << "Error adding edge!\n";
                            continue;
                        }
                        else { edge_to_update = new_edge; }
                    }
                }

                // if (debug_mode) std::cerr << "Check collision pair\n";
                // check if the intersecting volume has been accounted for
                if (!scene_support_graph[edge_to_update].collision_pair_exists(shape_index_lower, shape_index_upper) )
                {
                    // if (debug_mode) std::cerr << "Adding collision pair information\n";
                    scene_support_graph[edge_to_update].add_pair(shape_index_lower, shape_index_upper);
                    scene_support_graph[object_u].colliding_volume_ += total_volume_penetration;
                    scene_support_graph[supported_object].colliding_volume_ += total_volume_penetration;
                }
                // if (debug_mode) std::cerr << "Done\n";
            }
        }
    }
    // std::cerr << "Assigning ground supported vertices\n";
    // Assign vertices that are supported by ground
    vertex_t ground_vertex = vertex_map["background"];
    scene_support_graph[ground_vertex].ground_supported_ = true;
    std::cerr << "Background support contribution= " << scene_support_graph[ground_vertex].support_contributions_ << std::endl;
    assignAllConnectedToParentVertices(scene_support_graph,ground_vertex);
    // std::cerr << "Returning graph\n";
    return scene_support_graph;
}
