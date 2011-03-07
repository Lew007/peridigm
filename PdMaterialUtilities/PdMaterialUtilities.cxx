/*
 * PdMaterialUtilities.cxx
 *
 */
#include "PdMaterialUtilities.h"
#include <math.h>
#include <iostream>

namespace PdMaterialUtilities {


void updateGeometry
(
		const double* xOverlap,
		const double* uOverlap,
		const double* velocityOverlap,
		double* yOverlap,
		int overLapLength,
		double dt
)
{
	const double* x = xOverlap;
	const double* u = uOverlap;
	const double* v = velocityOverlap;
	double*       y = yOverlap;

	int length = overLapLength;
	for(;x!=xOverlap+length;x++,u++,v++,y++)
		*y = *x + *u + *v * dt;
}

void computeWeightedVolume
(
		const double* xOverlap,
		const double* volumeOverlap,
		double *mOwned,
		int myNumPoints,
		const int* localNeighborList
){
	double *m = mOwned;
	const double *xOwned = xOverlap;
	double cellVolume;
	const int *neighPtr = localNeighborList;
	for(int p=0;p<myNumPoints;p++, xOwned+=3, m++){
		int numNeigh = *neighPtr; neighPtr++;
		const double *X = xOwned;
		*m=0;
		for(int n=0;n<numNeigh;n++,neighPtr++){
			int localId = *neighPtr;
			cellVolume = volumeOverlap[localId];
			const double *XP = &xOverlap[3*localId];
			double dx = XP[0]-X[0];
			double dy = XP[1]-X[1];
			double dz = XP[2]-X[2];
			*m+=(dx*dx+dy*dy+dz*dz)*cellVolume;
		}
	}
}


void computeDilatation
(
		const double* xOverlap,
		const double* yOverlap,
		const double *mOwned,
		const double* volumeOverlap,
		const double* bondDamage,
		double* dilatationOwned,
		const int* localNeighborList,
		int numOwnedPoints
)
{
	double OMEGA=1.0;
	const double *xOwned = xOverlap;
	const double *yOwned = yOverlap;
	const double *m = mOwned;
	const double *v = volumeOverlap;
	double *theta = dilatationOwned;
	double cellVolume;
	const int *neighPtr = localNeighborList;
	for(int p=0; p<numOwnedPoints;p++, xOwned+=3, yOwned+=3, m++, theta++){
		int numNeigh = *neighPtr; neighPtr++;
		const double *X = xOwned;
		const double *Y = yOwned;
		*theta = 0;
		for(int n=0;n<numNeigh;n++,neighPtr++,bondDamage++){
			int localId = *neighPtr;
			cellVolume = v[localId];
			const double *XP = &xOverlap[3*localId];
			const double *YP = &yOverlap[3*localId];
			double dx = XP[0]-X[0];
			double dy = XP[1]-X[1];
			double dz = XP[2]-X[2];
			double zetaSqared = dx*dx+dy*dy+dz*dz;
			dx = YP[0]-Y[0];
			dy = YP[1]-Y[1];
			dz = YP[2]-Y[2];
			double dY = dx*dx+dy*dy+dz*dz;
			double d = sqrt(zetaSqared);
			double e = sqrt(dY)-d;
			*theta += 3.0*OMEGA*(1.0-*bondDamage)*d*e*cellVolume/(*m);
		}

	}
}

//! Version of computeDilataion for arbitrary list of ownedIDs
void computeDilatation
(
		const double* xOverlap,
		const double* yOverlap,
		const double *mOwned,
		const double* volumeOverlap,
		const double* bondDamage,
		double* dilatationOwned,
        const int* ownedIDs,
		const int* localNeighborList,
		int numOwnedPoints
)
{
	double OMEGA=1.0;
	const double *xOwned = xOverlap;
	const double *yOwned = yOverlap;
	const double *m = mOwned;
	const double *v = volumeOverlap;
	double *theta = dilatationOwned;
	double cellVolume;
	const int *neighPtr = localNeighborList;
	for(int p=0; p<numOwnedPoints;p++){

        int ID = ownedIDs[p];
        xOwned = &xOverlap[3*ID];
        yOwned = &yOverlap[3*ID];
        m = &mOwned[ID];
        theta = &dilatationOwned[ID];

		int numNeigh = *neighPtr; neighPtr++;
		const double *X = xOwned;
		const double *Y = yOwned;
		*theta = 0;
		for(int n=0;n<numNeigh;n++,neighPtr++,bondDamage++){
			int localId = *neighPtr;
			cellVolume = v[localId];
			const double *XP = &xOverlap[3*localId];
			const double *YP = &yOverlap[3*localId];
			double dx = XP[0]-X[0];
			double dy = XP[1]-X[1];
			double dz = XP[2]-X[2];
			double zetaSqared = dx*dx+dy*dy+dz*dz;
			dx = YP[0]-Y[0];
			dy = YP[1]-Y[1];
			dz = YP[2]-Y[2];
			double dY = dx*dx+dy*dy+dz*dz;
			double d = sqrt(zetaSqared);
			double e = sqrt(dY)-d;
			*theta += 3.0*OMEGA*(1.0-*bondDamage)*d*e*cellVolume/(*m);
		}

	}
}


void computeInternalForceLinearElastic
(
		const double* xOverlap,
		const double* yOverlap,
		const double* mOwned,
		const double* volumeOverlap,
		const double* dilatationOwned,
		const double* bondDamage,
		double* fInternalOverlap,
		const int*  localNeighborList,
		int numOwnedPoints,
		double BULK_MODULUS,
		double SHEAR_MODULUS
)
{

	/*
	 * Compute processor local contribution to internal force
	 */
	double K = BULK_MODULUS;
	double MU = SHEAR_MODULUS;
	double OMEGA=1.0;

	const double *xOwned = xOverlap;
	const double *yOwned = yOverlap;
	const double *m = mOwned;
	const double *v = volumeOverlap;
	const double *theta = dilatationOwned;
	double *fOwned = fInternalOverlap;

	const int *neighPtr = localNeighborList;
	double cellVolume, alpha, dx, dy, dz, zeta, dY, t;
	for(int p=0;p<numOwnedPoints;p++, xOwned +=3, yOwned +=3, fOwned+=3, m++, theta++){

		int numNeigh = *neighPtr; neighPtr++;
		const double *X = xOwned;
		const double *Y = yOwned;
		alpha = 15.0*MU/(*m);
		double selfCellVolume = v[p];
		double c1 = OMEGA*(*theta)*(9.0*K-15.0*MU)/(3.0*(*m));
		for(int n=0;n<numNeigh;n++,neighPtr++,bondDamage++){
			int localId = *neighPtr;
			cellVolume = v[localId];
			const double *XP = &xOverlap[3*localId];
			const double *YP = &yOverlap[3*localId];
			dx = XP[0]-X[0];
			dy = XP[1]-X[1];
			dz = XP[2]-X[2];
			zeta = sqrt(dx*dx+dy*dy+dz*dz);
			dx = YP[0]-Y[0];
			dy = YP[1]-Y[1];
			dz = YP[2]-Y[2];
			dY = sqrt(dx*dx+dy*dy+dz*dz);
			t = (1.0-*bondDamage)*(c1 * zeta + (1.0-*bondDamage) * OMEGA * alpha * (dY - zeta));
			double fx = t * dx / dY;
			double fy = t * dy / dY;
			double fz = t * dz / dY;

			*(fOwned+0) += fx*cellVolume;
			*(fOwned+1) += fy*cellVolume;
			*(fOwned+2) += fz*cellVolume;
			fInternalOverlap[3*localId+0] -= fx*selfCellVolume;
			fInternalOverlap[3*localId+1] -= fy*selfCellVolume;
			fInternalOverlap[3*localId+2] -= fz*selfCellVolume;
		}

	}
}


void computeInternalForceLinearElastic
(
		const double* xOverlap,
		const double* yOverlap,
		const double* mOwned,
		const double* volumeOverlap,
		const double* dilatationOwned,
		const double* bondDamage,
		double* fInternalOverlap,
		const int*  ownedIDs,
		const int*  localNeighborList,
		int numOwnedPoints,
		double BULK_MODULUS,
		double SHEAR_MODULUS
)
{

	/*
	 * Compute processor local contribution to internal force
	 */
	double K = BULK_MODULUS;
	double MU = SHEAR_MODULUS;
	double OMEGA=1.0;

	const double *xOwned = xOverlap;
	const double *yOwned = yOverlap;
	const double *m = mOwned;
	const double *v = volumeOverlap;
	const double *theta = dilatationOwned;
	double *fOwned = fInternalOverlap;

	const int *neighPtr = localNeighborList;
	double cellVolume, alpha, dx, dy, dz, zeta, dY, t;
	for(int p=0;p<numOwnedPoints;p++){

        int ID = ownedIDs[p];
        xOwned = &xOverlap[3*ID];
        yOwned = &yOverlap[3*ID];
        fOwned = &fInternalOverlap[3*ID];
        m = &mOwned[ID];
        theta = &dilatationOwned[ID];

		int numNeigh = *neighPtr; neighPtr++;
		const double *X = xOwned;
		const double *Y = yOwned;
		alpha = 15.0*MU/(*m);
		double selfCellVolume = v[p];
		double c1 = OMEGA*(*theta)*(9.0*K-15.0*MU)/(3.0*(*m));
		for(int n=0;n<numNeigh;n++,neighPtr++,bondDamage++){
			int localId = *neighPtr;
			cellVolume = v[localId];
			const double *XP = &xOverlap[3*localId];
			const double *YP = &yOverlap[3*localId];
			dx = XP[0]-X[0];
			dy = XP[1]-X[1];
			dz = XP[2]-X[2];
			zeta = sqrt(dx*dx+dy*dy+dz*dz);
			dx = YP[0]-Y[0];
			dy = YP[1]-Y[1];
			dz = YP[2]-Y[2];
			dY = sqrt(dx*dx+dy*dy+dz*dz);
			t = (1.0-*bondDamage)*(c1 * zeta + (1.0-*bondDamage) * OMEGA * alpha * (dY - zeta));
			double fx = t * dx / dY;
			double fy = t * dy / dY;
			double fz = t * dz / dY;

			*(fOwned+0) += fx*cellVolume;
			*(fOwned+1) += fy*cellVolume;
			*(fOwned+2) += fz*cellVolume;
			fInternalOverlap[3*localId+0] -= fx*selfCellVolume;
			fInternalOverlap[3*localId+1] -= fy*selfCellVolume;
			fInternalOverlap[3*localId+2] -= fz*selfCellVolume;
		}

	}
}


void computeInternalForceIsotropicElasticPlastic
(
		const double* xOverlap,
		const double* yOverlap,
		const double* mOwned,
		const double* volumeOverlap,
		const double* dilatationOwned,
		const double* bondDamage,
		const double* deviatoricPlasticExtensionStateN,
		double* deviatoricPlasticExtensionStateNp1,
		const double* lambdaN,
		double* lambdaNP1,
		double* fInternalOverlap,
		const int*  localNeighborList,
		int numOwnedPoints,
		double BULK_MODULUS,
		double SHEAR_MODULUS,
		double HORIZON,
		double yieldStress
)
{

	/*
	 * Compute processor local contribution to internal force
	 */
	double K = BULK_MODULUS;
	double MU = SHEAR_MODULUS;
	double OMEGA=1.0;
	double DELTA=HORIZON;

	const double *xOwned = xOverlap;
	const double *yOwned = yOverlap;
	const double *m = mOwned;
	const double *v = volumeOverlap;
	const double *theta = dilatationOwned;
	double *fOwned = fInternalOverlap;

	const int *neighPtr = localNeighborList;
	double cellVolume, alpha, dx, dy, dz, zeta, dY, t, ti, td, ed, edpN, tdTrial;
	for(int p=0;p<numOwnedPoints;p++, xOwned +=3, yOwned +=3, fOwned+=3, m++, theta++, lambdaN++, lambdaNP1++){

		int numNeigh = *neighPtr; neighPtr++;
		const double *X = xOwned;
		const double *Y = yOwned;
		double weightedVol = *m;
		alpha = 15.0*MU/weightedVol;
		double selfCellVolume = v[p];
		double c = 3 * K * (*theta) * OMEGA / weightedVol;
		double deltaLambda=0.0;

		/*
		 * Compute norm of trial stress
		 */
		double tdNorm = 0.0;
		tdNorm = computeDeviatoricForceStateNorm(numNeigh,*theta,neighPtr,bondDamage,deviatoricPlasticExtensionStateN,X,Y,xOverlap,yOverlap,v,alpha,OMEGA);

		/*
		 * Evaluate yield function
		 */
		/*
		 * 3d variety of yield value
		 */
		double yieldValue = 75.0 * yieldStress * yieldStress / 8 / M_PI / pow(DELTA,5);
		/*
		 * Planar variety of yield value
		 */
//		double THICKNESS=1.0;
//		double yieldValue = 225.0 * yieldStress * yieldStress / 8 / M_PI / THICKNESS / pow(DELTA,4);
//		double yieldValue = 0.5 * pow(15*yieldStress/weightedVol,2) * M_PI * THICKNESS * pow(DELTA,4) / 16.0;
		double f = tdNorm * tdNorm / 2 - yieldValue;
		bool elastic = true;

//		std::cout << "Point id = " << p << std::endl;
//		std::cout << "\tyieldStress/m^(4/5) = " << yieldStress/pow(weightedVol,4/5) << std::endl;
//		std::cout << "\tYield Value = " << yieldValue << "; tdNorm * tdNorm / 2 = " << tdNorm * tdNorm / 2 << std::endl;
		if(f>0){
			/*
			 * This step is incrementally plastic
			 */
			//			std::cout << "\t PLASTIC" << std::endl;
			elastic = false;
			deltaLambda=( tdNorm / sqrt(2.0*yieldValue) - 1.0 ) / alpha;
			*lambdaNP1 = *lambdaN + deltaLambda;
		} else {
//			std::cout << "\t ELASTIC" << std::endl;
			*lambdaNP1 = *lambdaN;
		}

		for(int n=0;n<numNeigh;n++,neighPtr++,bondDamage++, deviatoricPlasticExtensionStateN++, deviatoricPlasticExtensionStateNp1++){
			int localId = *neighPtr;
			cellVolume = v[localId];
			const double *XP = &xOverlap[3*localId];
			const double *YP = &yOverlap[3*localId];
			dx = XP[0]-X[0];
			dy = XP[1]-X[1];
			dz = XP[2]-X[2];
			zeta = sqrt(dx*dx+dy*dy+dz*dz);
			dx = YP[0]-Y[0];
			dy = YP[1]-Y[1];
			dz = YP[2]-Y[2];
			dY = sqrt(dx*dx+dy*dy+dz*dz);
			/*
			 * Deviatoric extension state
			 */
			ed = dY-zeta-*theta*zeta/3;

			/*
			 * Deviatoric plastic extension state from last step
			 */
			edpN = *deviatoricPlasticExtensionStateN;

			/*
			 * Compute trial stress
			 */
			tdTrial = alpha * OMEGA * (ed - edpN);

			/*
			 * Evaluate yield function
			 */
			if(elastic){
				/*
				 * Elastic case
				 */
				td = tdTrial;

				/*
				 * Therefore edpNp1 = edpN
				 */
				*deviatoricPlasticExtensionStateNp1 = *deviatoricPlasticExtensionStateN;

			} else {
				/*
				 * Compute deviatoric force state
				 */
				td = sqrt(2.0*yieldValue) * tdTrial / tdNorm;

				/*
				 * Update deviatoric plastic deformation state
				 */
				*deviatoricPlasticExtensionStateNp1 = edpN + td * deltaLambda;

//				std::cout << "Neighbor Id = " << localId << "; Updating deviatoricPlasticExtensionState = " << *deviatoricPlasticExtensionState << std::endl;
			}
//			std::cout << "\tNeighbor Id = " << localId << "\n\ttd = " << td;
			/*
			 * Compute isotropic part of force state
			 */
			ti = c * zeta;

			/*
			 * Force state (with damage)
			 */
			double d=(1.0-*bondDamage);
			t = d*(ti + d*td);

			/*
			 * Assemble pair wise force function
			 */
			double fx = t * dx / dY;
			double fy = t * dy / dY;
			double fz = t * dz / dY;

			*(fOwned+0) += fx*cellVolume;
			*(fOwned+1) += fy*cellVolume;
			*(fOwned+2) += fz*cellVolume;
			fInternalOverlap[3*localId+0] -= fx*selfCellVolume;
			fInternalOverlap[3*localId+1] -= fy*selfCellVolume;
			fInternalOverlap[3*localId+2] -= fz*selfCellVolume;
		}

	}
}

/**
 * Computes norm of deviatoric force state at a particular point
 * @param numNeigh -- number of neighbors at point
 * @param theta    -- dilatation at point
 * @param neighPtr -- list of neighbors at point
 * @param bondDamage     -- damage parameter for each bond at point
 * @param X              -- original coordinates of point
 * @param Y              -- current coordinates of point
 * @param xOverlap       -- pointer to overlap vector of original coordinates; use this to get neighbor original coordinates
 * @param yOverlap       -- pointer to overlap vector of current coordinates; use this to get neighbor current coordinates
 * @param volumeOverlap  -- pointer to volume overlap vector; use this to get volume of neighboring points
 * @param alpha          -- material property (alpha = 15 mu / m
 * @param OMEGA          -- weight function at point
 */
double computeDeviatoricForceStateNorm
(
		int numNeigh,
		double theta,
		const int *neighPtr,
		const double *bondDamage,
		const double *deviatoricPlasticExtensionState,
		const double *X,
		const double *Y,
		const double *xOverlap,
		const double *yOverlap,
		const double *volumeOverlap,
		double alpha,
		double OMEGA
)
{
	double norm=0.0;
	const double *v = volumeOverlap;
	double cellVolume, dx, dy, dz, zeta, dY, ed, edpN, tdTrial;

	for(int n=0;n<numNeigh;n++, neighPtr++, bondDamage++, deviatoricPlasticExtensionState++){
		int localId = *neighPtr;
		cellVolume = v[localId];
		const double *XP = &xOverlap[3*localId];
		const double *YP = &yOverlap[3*localId];
		dx = XP[0]-X[0];
		dy = XP[1]-X[1];
		dz = XP[2]-X[2];
		zeta = sqrt(dx*dx+dy*dy+dz*dz);
		dx = YP[0]-Y[0];
		dy = YP[1]-Y[1];
		dz = YP[2]-Y[2];
		dY = sqrt(dx*dx+dy*dy+dz*dz);

		/*
		 * Deviatoric extension state
		 */
		ed = dY-zeta-theta*zeta/3;

		/*
		 * Deviatoric plastic extension state from last step
		 */
		edpN = *deviatoricPlasticExtensionState;

		/*
		 * Compute trial stress
		 * NOTE: include damage
		 */
		double d=(1.0-*bondDamage);
		tdTrial = d * alpha * OMEGA * (ed - edpN);

		/*
		 * Accumulate norm
		 */
		norm += tdTrial * tdTrial * cellVolume;


//		std::cout << "computeDeviatoricForceStateNorm \n\talpha = " << alpha << "\n\tOMEGA = "<< OMEGA << std::endl;
//		std::cout << "computeDeviatoricForceStateNorm \n\tdY = " << dY <<"\n\tzeta = " << zeta << "\n\ttheta =  "<< theta << std::endl;
//		std::cout << "computeDeviatoricForceStateNorm \n\ted = " << dY-zeta-theta*zeta/3 << "\n\tedpN = " << edpN << std::endl;

	}

	return sqrt(norm);
}

void computeShearCorrectionFactor
(
		int numOwnedPoints,
		const double *xOverlap,
		const double *volumeOverlap,
		const int*  localNeighborList,
		double horizon,
		double *shearCorrectionFactorOwned
){

	const int *neighPtr = localNeighborList;
	const double *xOwned = xOverlap;
	double *scaleFactor = shearCorrectionFactorOwned;
	for(int p=0;p<numOwnedPoints;p++, xOwned+=3, scaleFactor++){
		int numNeigh = *neighPtr; neighPtr++;
		const double *X = xOwned;
		*scaleFactor=probeShearModulusScaleFactor(numNeigh,neighPtr,X,xOverlap,volumeOverlap,horizon);
		neighPtr+=numNeigh;
	}

}

double probeShearModulusScaleFactor
(
		int numNeigh,
		const int *neighPtr,
		const double *X,
		const double *xOverlap,
		const double *volumeOverlap,
		double horizon
)
{
	double gamma=1.0e-6;
	double reference = 4.0 * M_PI * gamma * gamma * pow(horizon,5) / 75.0;
	const double *v = volumeOverlap;
	double cellVolume, dx, dy, dz, zeta, dY, ed;
	/*
	 * Pure shear centered at X
	 * X has no displacement
	 */
	double Y[] = {*X, *(X+1), *(X+2)};
	double YP[3];
	double norm=0.0;
	for(int n=0;n<numNeigh;n++, neighPtr++){
		int localId = *neighPtr;
		cellVolume = v[localId];
		const double *XP = &xOverlap[3*localId];
		dx = XP[0]-X[0];
		dy = XP[1]-X[1];
		dz = XP[2]-X[2];
		zeta = sqrt(dx*dx+dy*dy+dz*dz);
		/*
		 * Pure shear: XY
		 */
		YP[0] = XP[0] + gamma * dx;
		YP[1] = XP[1];
		YP[2] = XP[2];

		/*
		 * Deformation State
		 */
		dx = YP[0]-Y[0];
		dy = YP[1]-Y[1];
		dz = YP[2]-Y[2];
		dY = sqrt(dx*dx+dy*dy+dz*dz);

		/*
		 * Deviatoric extension state
		 * NOTE that this is under the assumption of pure shear
		 */
		ed = dY-zeta;

		/*
		 * Accumulate norm
		 */
		norm += ed * ed * cellVolume;

	}

	return reference/norm;
}

}
