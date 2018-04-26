#ifndef MATH_FISHER_H_
#define MATH_FISHER_H_

#include <iostream>
#include <cmath>
#include <vector>
#include <cerrno>
#include "../support/type_definitions.h"

namespace tachyon{
namespace math{

#define KF_GAMMA_EPS 1e-14
#define KF_TINY 1e-290
#define FISHER_TINY 1e-279
#define STIRLING_CONSTANT 0.5 * log(2 * M_PI)

class Fisher{

public:
	Fisher() :
		number(1000+1),
		logN_values(1000, 0)
	{
		this->Build();
	}

	Fisher(const U32 number) :
		number(number+1),
		logN_values(number, 0)
	{
		this->Build();
	}

	~Fisher(){}

	void Build(void){
		double factorial = 0;
		this->logN_values[0] = 0;
		for(U32 i = 1; i < this->number; ++i){
			factorial += log(i);
			this->logN_values[i] = factorial;
		}
	}

	/* Log gamma function
	 * \log{\Gamma(z)}
	 * AS245, 2nd algorithm, http://lib.stat.cmu.edu/apstat/245
	 */
	double kf_lgamma(double z) const{
		double x = 0;
		x += 0.1659470187408462e-06 / (z+7);
		x += 0.9934937113930748e-05 / (z+6);
		x -= 0.1385710331296526     / (z+5);
		x += 12.50734324009056      / (z+4);
		x -= 176.6150291498386      / (z+3);
		x += 771.3234287757674      / (z+2);
		x -= 1259.139216722289      / (z+1);
		x += 676.5203681218835      / z;
		x += 0.9999999999995183;
		return log(x) - 5.58106146679532777 - z + (z-0.5) * log(z+6.5);
	}

	// regularized lower incomplete gamma function, by series expansion
	double _kf_gammap(double s, double z) const{
		double sum, x;
		int k;
		for (k = 1, sum = x = 1.; k < 100; ++k) {
			sum += (x *= z / (s + k));
			if (x / sum < KF_GAMMA_EPS) break;
		}
		return exp(s * log(z) - z - kf_lgamma(s + 1.) + log(sum));
	}

	// regularized upper incomplete gamma function, by continued fraction
	double _kf_gammaq(double s, double z) const{
		int j;
		double C, D, f;
		f = 1. + z - s; C = f; D = 0.;
		// Modified Lentz's algorithm for computing continued fraction
		// See Numerical Recipes in C, 2nd edition, section 5.2
		for (j = 1; j < 100; ++j) {
			double a = j * (s - j), b = (j<<1) + 1 + z - s, d;
			D = b + a * D;
			if (D < KF_TINY) D = KF_TINY;
			C = b + a / C;
			if (C < KF_TINY) C = KF_TINY;
			D = 1. / D;
			d = C * D;
			f *= d;
			if (fabs(d - 1.) < KF_GAMMA_EPS) break;
		}
		return exp(s * log(z) - z - kf_lgamma(s) - log(f));
	}

	inline double kf_gammap(double s, double z) const{return z <= 1. || z < s? _kf_gammap(s, z) : 1. - _kf_gammaq(s, z);}
	inline double kf_gammaq(double s, double z) const{return z <= 1. || z < s? 1. - _kf_gammap(s, z) : _kf_gammaq(s, z);}


	__attribute__((always_inline))
	inline double StirlingsApproximation(const double v) const{
		return(STIRLING_CONSTANT + (v - 0.5) * log(v) - v + 1/(12*v) + 1/(360 * v * v * v));
	}

	__attribute__((always_inline))
	inline double logN(const S32& value) const{
		//if(value < this->number)
		//	return this->logN_values[value];
		//else
		//return this->StirlingsApproximation((double)value + 1);
		return(StirlingsApproximation((double)value+1));
	}

	__attribute__((always_inline))
	inline double fisherTest(const S32& a, const S32& b, const S32& c, const S32 d) const{
		// Rewrite Fisher's 2x2 test in log form
		// return e^x
		return(exp(logN(a+b) + logN(c+d) + logN(a+c) + logN(b+d) - logN(a) - logN(b) - logN(c) - logN(d) - logN(a + b + c + d)));
	}


	double fisherTestLess(S32 a, S32 b, S32 c, S32 d) const{
		S32 minValue = a;
		if(d < minValue) minValue = d;

		if(minValue > 50)
			return(this->chisqr(1, this->chiSquaredTest(a,b,c,d)));

		double sum = 0;
		for(S32 i = 0; i <= minValue; ++i){
			sum += this->fisherTest(a, b, c, d);
			if(sum < FISHER_TINY) break;
			--a, ++b, ++c, --d;
		}

		return(sum);
	}

	double fisherTestGreater(S32 a, S32 b, S32 c, S32 d) const{
		S32 minValue = b;
		if(c < minValue) minValue = c;

		if(minValue > 50)
			return(this->chisqr(1, this->chiSquaredTest(a,b,c,d)));

		double sum = 0;
		for(S32 i = 0; i <= minValue; ++i){
			sum += this->fisherTest(a, b, c, d);
			if(sum < FISHER_TINY) break;
			++a, --b, --c, ++d;
		}

		return(sum);
	}

	double chisqr(const S32& Dof, const double& Cv) const{
		if(Cv < 0 || Dof < 1)
			return 0.0;

		const double K = ((double)Dof) * 0.5;
		const double X = Cv * 0.5;
		if(Dof == 2)
			return exp(-1.0 * X);

	   return(this->kf_gammaq(K, X));
	}

	template <class T>
	double chiSquaredTest(T& a, T& b, T& c, T& d) const{
		const T rowSums[2] = {a+b, c+d};
		const T colSums[2] = {a+c, b+d};
		const double total = a + b + c + d;
		double adjustValue = 0;
		if(a > 0.5 && b > 0.5 && c > 0.5 && d > 0.5)
			adjustValue = 0.5;

		const double chisq = ((pow(a - (double)rowSums[0]*colSums[0]/total,2)-adjustValue)/(rowSums[0]*colSums[0]/total) +
							  (pow(b - (double)rowSums[0]*colSums[1]/total,2)-adjustValue)/(rowSums[0]*colSums[1]/total) +
							  (pow(c - (double)rowSums[1]*colSums[0]/total,2)-adjustValue)/(rowSums[1]*colSums[0]/total) +
							  (pow(d - (double)rowSums[1]*colSums[1]/total,2)-adjustValue)/(rowSums[1]*colSums[1]/total) );

		if(chisq < 0){
			return ((pow(a - (double)rowSums[0]*colSums[0]/total,2))/(rowSums[0]*colSums[0]/total) +
					(pow(b - (double)rowSums[0]*colSums[1]/total,2))/(rowSums[0]*colSums[1]/total) +
					(pow(c - (double)rowSums[1]*colSums[0]/total,2))/(rowSums[1]*colSums[0]/total) +
					(pow(d - (double)rowSums[1]*colSums[1]/total,2))/(rowSums[1]*colSums[1]/total) );
		}
		return chisq;
	}

private:
	U32 number;
	std::vector<double> logN_values;
};


}
}



#endif /* MATH_FISHER_H_ */
