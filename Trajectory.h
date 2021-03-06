
#ifndef _TRAJECTORY_H_
#define _TRAJECTORY_H_


#include <stdlib.h>
#include <vector>
#include "Pose.h"
//#include "Renderer.h"
#include "matrix4.h"
#include "Vector4.h"
#include <Eigen/Geometry>

	enum cubicSpline{catmullRom, bSpline};
	enum Cycle{circle, pendulum, clamp};
	
class Trajectory{
	private:
		Eigen::MatrixXf M;
		int multiplier;
		int startFrame;
		int endFrame;
		int u;
		unsigned int cpIndex;
		bool forward;		//Used for pendulum splines
		bool end;			//Used for clamped splines
		float PofU[7];

		void generateControlPoints(){
			controlPoints.push_back(new PoseKey(-1.5,	0.0,	2.0,	0.0,	10.0,	0.0,	 0));
			controlPoints.push_back(new PoseKey(-1.0,	1.0,	1.0,	0.0,	0.0,	0.0,	30));
			controlPoints.push_back(new PoseKey(-0.5,	1.0,	0.5,	0.0,	0.0,	0.0,	60));
			controlPoints.push_back(new PoseKey(0.0,	0.0,	-1.5,	0.0,	45.0,	0.0,	90));
			controlPoints.push_back(new PoseKey(0.5,	0.0,	-1.5,	0.0,	0.0,	45.0,	120));
			controlPoints.push_back(new PoseKey(1.0,	0.0,	-.5,	0.0,	0.0,	45.0,	150));
			//controlPoints.push_back(new PoseKey(1.5,	0.0,	1.0,	0.0,	0.0,	45.0,	180));
			controlPoints.push_back(new PoseKey(1.5,	0.0,	1.0,	0.0,	0.0,	45.0,	250));

		}

		void resetPath(vector<PoseKey*> newControlPoints){
			controlPoints.clear();
			controlPoints = newControlPoints;
		}

		void setSplineType(){
			M.resize(4,4);
			//Catmull Rom blending function
			if(splineType == catmullRom){
				M(0,0) = -0.5; M(0, 1) = 1.5; M(0, 2)= -1.5; M(0,3) = 0.5;
				M(1,0) =  1.0; M(1, 1) = -2.5; M(1, 2) = 2.0; M(1,3) = -0.5;
				M(2,0) =  -0.5; M(2, 1) = 0.0; M(2, 2) = 0.5; M(2,3) = 0.0;
				M(3,0) =  0.0; M(3, 1) = 1.0; M(3, 2) = 0.0; M(3,3) = 0.0;
			}else if(splineType == bSpline){
				//B-Spline Blending Function
				printf("Interpolating a B-Spline\n");
				M(0,0) = -1.0; M(0, 1) = 3.0; M(0, 2)= -3.0; M(0,3) = 1.0;
				M(1,0) =  3.0; M(1, 1) = -6.0; M(1, 2) = 3.0; M(1,3) = 0.0;
				M(2,0) =  -3.0; M(2, 1) = 0.0; M(2, 2) = 3.0; M(2,3) = 0.0;
				M(3,0) =  1.0; M(3, 1) = 4.0; M(3, 2) = 1.0; M(3,3) = 0.0;
				M = M * 1.0/6.0;
			}
		}

		int clampIndex(int index){
			if(index < 0)
				return 0;
			else if(index > controlPoints.size() -1)
				return controlPoints.size() -1;
			else
				return index;
		}

	public:
		vector<PoseKey*> controlPoints;
		cubicSpline splineType;
		Cycle cycleType;
		vector<vertex3*> pointsAlongPath;

		void init(){
			u = 0;	//local frame
			cpIndex = 1;
			//curFrame = globalFrame?
			forward = true;
			end = false;
			M = Eigen::MatrixXf(4, 4);
			endFrame = controlPoints.at(1)->frame;
			startFrame = controlPoints.at(0)->frame;
			setSplineType();
			multiplier = 0;
		}

		Trajectory(){
			generateControlPoints();
			splineType = catmullRom;
			cycleType = pendulum;
			init();
		}

		Trajectory(vector<PoseKey*> cps){
			controlPoints = cps;
			splineType = catmullRom;
			cycleType = pendulum;
			init();
			
		}

		Trajectory(cubicSpline spline){
			generateControlPoints();
			splineType = spline;
			cycleType = pendulum;
			init();
		}

		Trajectory(int cycleType){
			this->cycleType = (Cycle)cycleType;
			generateControlPoints();
			splineType = catmullRom;
			init();
		}

		Trajectory(cubicSpline spline, Cycle cycle){
			this->cycleType = cycle;
			generateControlPoints();
			splineType = spline;
			init();
		}
		
		Trajectory(Cycle cycleType){
			this->cycleType = cycleType;
			generateControlPoints();
			splineType = catmullRom;
			init();
		}

		~Trajectory(){
			while(!controlPoints.empty()) delete controlPoints.back(), controlPoints.pop_back();
			while(!pointsAlongPath.empty()) delete pointsAlongPath.back(), pointsAlongPath.pop_back();
		}

		bool isCalculated(){
			if(end){
				return true;
			}else
				return false;
		}
		
		void determineIndices(int &k0, int &k1, int &k2, int &k3){
			//circle, pendulum, clamp
			switch(cycleType){
				case circle:{
					printf("CpIndex = %d ----- u = %d ----- startFrame = %d -------- endFrame = %d \n", cpIndex, u, startFrame, endFrame);
					//Must use brackets because we define a variable for scope reasons
					int size = controlPoints.size();
					//cpIndex = cpIndex % size;
					if(cpIndex == 0)
						k0 = size-1;
					else
						k0 = (cpIndex-1) % size;	//clampIndex should do nothing
					k1 = cpIndex;	//clampIndex should do nothing for all
					k2 = (cpIndex+1) % size;
					k3 = (cpIndex+2) % size;
				}
				break;
				case pendulum:
					if(forward){
						k0 = clampIndex(cpIndex - 2);
						k1 = clampIndex(cpIndex-1);
						k2 = clampIndex(cpIndex);
						k3 = clampIndex(cpIndex + 1);
						if((k2 == k3) && (u >= endFrame)){
							forward = false;
							end = true;
						}
					}else{
						k0 = clampIndex(cpIndex + 2);
						k1 = clampIndex(cpIndex +1);
						k2 = clampIndex(cpIndex);
						k3 = clampIndex(cpIndex - 1);
						if(k2 == k3 && u<=endFrame)
							forward = true;
					}
				break;
				case clamp:
					k0 = clampIndex(cpIndex - 1);
					k1 = clampIndex(cpIndex);
					k2 = clampIndex(cpIndex +1);
					k3 = clampIndex(cpIndex + 2);
					if(k1 == k2 && u>=endFrame)
						end = true;
				break;
				default:
					printf("No cycle Type is chosen");
				break;
			}
		}

		virtual Pose& update(){
			//If finished interpolating between current start and end Control Points
			//Move onto next one.
			//Also check that we have not reached the end of our list to avoid
			//out of bounds exceptions
			bool cp = false;
			if(forward)
				u++;			
			else
				u--;	//Should never be negative
			int k0, k1, k2, k3;
			k0 = k1 = k2 = k3 = 0;
			//implies forward because u is >
			if(cycleType!= circle && (u >= endFrame) && forward && cpIndex < (controlPoints.size() - 1)){
				startFrame = endFrame;
				cpIndex++;
				endFrame = controlPoints.at(clampIndex(cpIndex))->frame;
				u = startFrame;
				cp = true;
			}else if(cycleType != circle && (u <= endFrame) && !forward && cpIndex > 0){
				//Should only reach this point if moving backwards aka !forward
				startFrame = endFrame;
				cpIndex--;
				endFrame = controlPoints.at(clampIndex(cpIndex))->frame;
				u = startFrame;
				cp =true;
			}else if(cycleType != circle && (u >= endFrame) && forward){
				//do not have to check index, because it will be modded to stay safe
				startFrame = controlPoints.at(clampIndex(cpIndex))->frame;
				cpIndex++;
				endFrame = controlPoints.at(clampIndex(cpIndex))->frame;
				u = startFrame;
				cp = true;
			}else if(cycleType == circle && u>=endFrame){
				int size = controlPoints.size();
				int totalFrames = controlPoints.at(size-1)->frame;
				int avg = totalFrames/size;
				totalFrames += avg;
				startFrame = endFrame;
				cpIndex++;
				cpIndex = cpIndex % size;
				if(cpIndex ==0){
					multiplier++;
				}
				endFrame = controlPoints.at(clampIndex(cpIndex))->frame + (totalFrames * multiplier);
				cp = true;

				if(startFrame == totalFrames)
					end = true;
			}
			determineIndices(k0, k1, k2, k3);
			
			float u_norm;			//TODO rethink
			if(forward){
				u_norm = abs((float)(u -startFrame) / (float)(endFrame- startFrame));
				//printf("Forward U_Norm ------- %f--------------%d - %d/ (%d - %d) \n", u_norm, u, startFrame, endFrame, startFrame);
			}
			else{
				u_norm =  (float)(startFrame - u) / (startFrame- endFrame);
				//printf("Backward U_Norm ------- %f--------------%d - %d/ (%d - %d) \n", u_norm, startFrame, u, startFrame, startFrame);
			}
			Eigen::MatrixXf T(1, 4);
			T(0, 0) = (u_norm*u_norm*u_norm);
			T(0, 1) = (u_norm*u_norm);
			T(0, 2) = (u_norm);
			T(0, 3) = 1.0f; 
	
			//PRINT K's
			//printf("K0 -- %d--- k1-- %d --k2  %d--- k3-- %d\n", k0, k1, k2, k3);
			//Create G!
			Eigen::MatrixXf G(4, 1);
			for(unsigned int i=0; i<7; i++) {
					// G
					G(0, 0) = controlPoints.at(k0)[0][i];			//must add the [0] to dereference the pointer
					G(1, 0) = controlPoints.at(k1)[0][i];
					G(2, 0) = controlPoints.at(k2)[0][i];
					G(3, 0) = controlPoints.at(k3)[0][i];
					//G.print();		
					//printf("%d ---%f, %f, %f, %f\n", i, G(0,0), G(1, 0), G(2, 0), G(3,0));
					// interpolation
					
					Eigen::MatrixXf interpolation = T*M*G;				//multipying vectors and matrices
					PofU[i] = (float)interpolation(0,0);				//what do we do with this?
					
			//	printf("---%d %f %f\n", i, PofU[i], interpolation(0,0));
			//		cout << "The matrix T is:\n" << endl << T << endl;
			//		cout << "The matrix M is:\n" << endl << M << endl;
			//		cout << "The matrix G is:\n" << endl << G << endl;
			//		cout << "The matrix interpolation is:\n" << endl << interpolation << endl;
			}
			Pose* inbetween = new Pose(PofU[0], PofU[1], PofU[2], PofU[3], PofU[4], PofU[5], PofU[6]);
			if(!end && !cp )
				pointsAlongPath.push_back(&inbetween->position);
			return *inbetween;
		}

		void resetPath(){
			controlPoints.clear();
		}
		

	public:
	EIGEN_MAKE_ALIGNED_OPERATOR_NEW
};

#endif //Trajectory