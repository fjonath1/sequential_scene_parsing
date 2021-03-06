#ifndef SCENE_PHYSICS_ENGINE_H
#define SCENE_PHYSICS_ENGINE_H

#include <iostream>
#include <math.h>
#include <vector>
#include <map>
#include <boost/thread.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/graph/graphviz.hpp>

#include <BulletCollision/CollisionDispatch/btCollisionWorld.h>

// Rendering platform
#ifdef _WINDOWS
#include "debugdrawer/Win32DemoApplication.h"
#define PlatformDemoApplication Win32DemoApplication
#else
#include "debugdrawer/GlutDemoApplication.h"
#define PlatformDemoApplication GlutDemoApplication
#endif

#include "debugdrawer/GlutStuff.h"
#include "debugdrawer/GLDebugDrawer.h"

#include "object_data_property.h"

#include "scene_physics_penalty.h"
#include "scene_physics_support.h"
#include "scene_data_forces.h"

static void _worldTickCallback(btDynamicsWorld *world, btScalar timeStep);

struct MassProp
{
	MassProp() {}
	MassProp(const btScalar &inv_mass, const btVector3 &local_inertia): inertia(local_inertia)
	{
		mass = inv_mass > 0 ? 1/inv_mass : 0; 
	}

	btScalar mass;
	btVector3 inertia;
};

class PhysicsEngine : public PlatformDemoApplication
{
public:
	PhysicsEngine();
	virtual ~PhysicsEngine()
	{
		exitPhysics();
	}

	GLDebugDrawer gDebugDraw;
	// use plane as background (table)
	void addBackgroundPlane(btVector3 plane_normal, btScalar plane_constant, btVector3 plane_center);
	void addBackgroundConvexHull(const std::vector<btVector3> &plane_points, btVector3 plane_normal);
	void addBackgroundMesh(btTriangleMesh* trimesh, btVector3 plane_normal, btVector3 plane_center);

	// uses a frame that has Y direction up as a guide for gravity direction
	void setGravityVectorDirectionFromTfYUp(const btTransform &transform_y_is_inverse_gravity_direction);
	void setGravityVectorDirection(const btVector3 &gravity);
	void setGravityFromBackgroundNormal(const bool &input);
	btVector3 getGravityDirection() const;
	void addObjects(const std::vector<ObjectWithID> &objects);
	std::map<std::string, btTransform>  getUpdatedObjectPoses();
	std::map<std::string, btTransform>  getCurrentObjectPoses();
	void resetObjects(const bool &permanent_removal);

	void setObjectPenaltyDatabase(std::map<std::string, ObjectPenaltyParameters> * penalty_database);

	void setSimulationMode(const int &simulation_mode, const double simulation_step = 1./120,
		const unsigned int &number_of_world_tick = 100);
	// solver setting: check http://bulletphysics.org/mediawiki-1.5.8/index.php/BtContactSolverInfo
	void setPhysicsSolverSetting(const int &m_numIterations, const bool randomize_order = true, 
		const int &m_splitImpulse = 1, const btScalar &m_splitImpulsePenetrationThreshold = -0.02);

	void setDebugMode(bool debug);
	void renderingLaunched(const bool &flag = true);

	// Scene analysis
	void resetObjectMotionState(const bool &reset_object_pose, const std::map<std::string, btTransform> &target_pose_map);
	SceneSupportGraph getCurrentSceneGraph(std::map<std::string, vertex_t> &vertex_map);
	SceneSupportGraph getUpdatedSceneGraph(std::map<std::string, vertex_t> &vertex_map);
	void prepareSimulationForOneTestHypothesis(const std::string &object_id, const btTransform &object_pose, const bool &resetObjectPosition = true);
	void prepareSimulationForWithBestTestPose();
	void changeBestTestPoseMap(const std::string &object_id, const btTransform &object_pose);
	void changeBestTestPoseMap(const std::map<std::string, btTransform> &object_best_pose_from_data);
	btTransform getTransformOfBestData(const std::string &object_id, bool use_best_test_data = false) const;
	
	// vertex_t getObjectVertexFromSupportGraph(const std::string &object_name, btTransform &object_position);
	void stepSimulationWithoutEvaluation(const double & delta_time, const double &simulation_step, const bool &data_forces_enabled = true);
	void worldTickCallback(const btScalar &timeStep);

	void setFeedbackDataForcesGenerator(FeedbackDataForcesGenerator *data_forces_generator);

	std::map<std::string, btTransform>  getAssociatedBestPoseDataFromStringVector(
		const std::vector<std::string> &input, bool use_best_test_data = false);
	void removeAllRigidBodyFromWorld();
	void addExistingRigidBodyBackFromMap(const std::string &object_id, const btTransform &object_pose);
	void addExistingRigidBodyBackFromMap(const std::map<std::string, btTransform> &rigid_bodies);
	void removeExistingRigidBodyWithMap(const std::map<std::string, btTransform> &rigid_bodies);

	void setIgnoreDataForces(const std::string &object_id, bool value);
	void makeObjectStatic(const std::string &object_id, const bool &make_static);
	std::vector<std::string> getAllActiveObjectIds() const;

	void contactTest(btCollisionObject* col_object, btCollisionWorld::ContactResultCallback& result);

// Additional functions used for rendering:
	void initPhysics();
	void exitPhysics();

	virtual void clientMoveAndDisplay();

	virtual void displayCallback();
	virtual void clientResetScene();

	virtual void setCameraClippingPlaneNearFar(btScalar near, btScalar far = 10000.f);
	virtual void setCameraPositionAndTarget(btVector3 cam_position, btVector3 cam_target);

	static DemoApplication* Create()
	{
		PhysicsEngine* demo = new PhysicsEngine;
		// demo->myinit();
		demo->initPhysics();
		return demo;
	}

private:
	void simulate();
	bool checkSteadyState();
	void cacheObjectVelocities(const btScalar &timeStep);
	void stopAllObjectMotion();
	void applyDataForces();
	void makeStatic(btRigidBody &object, const bool &make_static);
	
	bool debug_messages_;
	bool have_background_;
	bool use_background_normal_as_gravity_;
	bool rendering_launched_;
	bool in_simulation_;
	bool enable_data_forces_;
	unsigned int world_tick_counter_;

	// rigid body data from ObjectWithID input with ID information
	std::map<std::string, btRigidBody*> rigid_body_;
	std::map<std::string, btTransform> object_best_pose_from_data_;

	std::map<btRigidBody*, MassProp> object_original_mass_prop_;
	std::map<std::string, bool> object_original_data_forces_flag_;

	// std::map<std::string, btTransform> object_test_pose_map_;
	std::map<std::string, btTransform> object_best_test_pose_map_;

	std::map<std::string, bool> ignored_data_forces_;

	btRigidBody* background_;
	btVector3 background_surface_normal_;

	// physics engine environment parameters
	btBroadphaseInterface* m_broadphase;
	btDefaultCollisionConfiguration* m_collisionConfiguration;
	btCollisionDispatcher* m_dispatcher;
	btSequentialImpulseConstraintSolver* m_solver;
	// DO NOT DECLARE m_dynamicworld here. It will break OPENGL simulation
	btAlignedObjectArray<btCollisionShape*> m_collisionShapes;
	
	std::map<std::string, ObjectPenaltyParameters> object_penalty_parameter_database_by_id_;
	std::map<std::string, ObjectPenaltyParameters> * object_penalty_parameter_database_;
	std::map<std::string, std::string> object_label_class_map_;
	
	double gravity_magnitude_;
	btVector3 gravity_vector_;
	btVector3 gravity_unit_vector_;
	
	std::map<std::string, MovementComponent> object_velocity_;
	std::map<std::string, MovementComponent> object_acceleration_;
	SceneSupportGraph scene_graph_;
	std::map<std::string, vertex_t> vertex_map_;

	btVector3 camera_coordinate_, target_coordinate_;
	double simulation_step_, fixed_step_;
	boost::mutex mtx_;

	bool reset_obj_vel_every_frame_;
	bool reset_interaction_forces_every_frame_;
	bool stop_simulation_after_have_support_graph_;
	bool skip_scene_evaluation_;
	unsigned int number_of_world_tick_;

	double best_scene_probability_;
	FeedbackDataForcesGenerator * data_forces_generator_;
};

struct OverlappingObjectSensor : public btCollisionWorld::ContactResultCallback
{
	OverlappingObjectSensor(btCollisionObject& col_obj, const std::string &object_name): 
		btCollisionWorld::ContactResultCallback(), body_(col_obj), object_id_(object_name),
		total_intersecting_volume_(0.), total_penetration_depth_(0.) {}

	btCollisionObject& body_;
	std::string object_id_;
	double total_intersecting_volume_;
	double total_penetration_depth_;
	double bounding_box_volume_;

	//! Called with each contact for your own processing (e.g. test if contacts fall in within sensor parameters)
	virtual btScalar addSingleResult(btManifoldPoint& cp,
		const btCollisionObjectWrapper* colObj0,int partId0,int index0,
		const btCollisionObjectWrapper* colObj1,int partId1,int index1)
	{
		const btCollisionObject* obj_0 = colObj0->getCollisionObject();
		const btCollisionObject* obj_1 = colObj1->getCollisionObject();

		btVector3 pt; // will be set to point of collision relative to body
		std::string other_id;
		bool other_object_is_1;
		if (colObj0->m_collisionObject==&body_)
		{
			pt = cp.m_localPointA;
			other_object_is_1 = true;
			other_id = getObjectIDFromCollisionObject(obj_1);
		}
		else
		{
			assert(colObj1->m_collisionObject==&body_ && "body does not match either collision object");
			pt = cp.m_localPointB;
			other_object_is_1 = false;
			other_id = getObjectIDFromCollisionObject(obj_0);
		}

        btAABB shapeAABB_0 = getCollisionAABB(colObj0->getCollisionObject(), cp, true, index0);
        btAABB shapeAABB_1 = getCollisionAABB(colObj1->getCollisionObject(), cp, false, index1);
		bounding_box_volume_ = other_object_is_1 ? getBoundingBoxVolume(shapeAABB_0) : getBoundingBoxVolume(shapeAABB_1);
		if (other_id == "unrecognized_object" || other_id == "background" || other_id == object_id_) return 0;

		// do stuff with the collision point
		total_penetration_depth_ += cp.getDistance() < 0 ? -cp.getDistance() : 0;
        
		
        total_intersecting_volume_ += getIntersectingVolume(shapeAABB_0,shapeAABB_1);
		
		return 0; // There was a planned purpose for the return value of addSingleResult, but it is not used so you can ignore it.
	}

	bool checkOverlapWithinThreshold(const double &max_depth_penetration, const double &max_volume_penetration)
	{
		return (total_penetration_depth_ < max_depth_penetration) && (total_intersecting_volume_ < max_volume_penetration);
	}
};

#endif
