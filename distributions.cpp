#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "distributions.h"

extern void error(char *format, ...);

/* gives a uniform(0,1) r.v. */
double uniform(long int *seed)		//u16807 rng
{ 
 *seed = (long int) fmod(*seed * 16807.0, 2147483647.0); 
 return (*seed / 2147483647.0); 
} 

double uniform_a_b(double low, double high, long int *seed)
{
	double unif, range, retval;
	
	unif = uniform(seed);
	range = high - low;
	retval = low + unif*range;
	return retval;
}

/* returns a r.v. from exp distribution with mean = mean */
double exponential(double mean, long int *seed)
{
	double unif;

	unif = uniform(seed);
	return (-1*mean*log(unif));
}

//method obtained from Law and Kelton
double normal(double mean, double sd, long int *seed)
{
	double u1, u2, v1, v2, w=2, y, std_normal, reg_normal;  //w=2 just for starting while loop;

	while (w > 1)
	{
		u1 = uniform(seed);
		u2 = uniform(seed);
		v1 = 2*u1 - 1;
		v2 = 2*u2 - 1;
		w = pow(v1,2) + pow(v2,2);
	}

	y = sqrt((-2*log(w))/w);

	std_normal = v1*y;
	reg_normal = mean + std_normal*sd;

	return reg_normal;
}

//from Law and Kelton
//generates a rv from a gamma(a1,1) distribution
double gamma_a1_1(double a1, long int *seed)
{
	double unif, b, y, p, u1, u2, a, q, d, theta, v, z, w;

	if (a1 == 1) //exponential distrib with mean = 1
	{
		return exponential(1,seed);
	}
	else if (a1 < 1)
	{
		b = (exp(1.0) + a1)/exp(1.0);
		//this will run until a value is returned
		while (1)
		{
			unif = uniform(seed);
			p = b*unif;
			if (p > 1)
			{
				y = -1*log((b-p)/a1);
				unif = uniform(seed);
				if (unif <= pow(y,a1-1))
					return y;
			}
			else
			{
				y = pow(p,(1/a1));
				unif = uniform(seed);
				if (unif <= exp(-y))
					return y;
			}
		}
		
		//should never get here
		error("Error in gamma_a1_1 (a1 < 1)\n");
		exit(0);
	}
	else //a1 > 1
	{
		a = 1/sqrt(2*a1 - 1);
		b = a1 - log(4.0);
		q = a1 + (1/a);
		theta = 4.5;
		d = 1 + log(theta);

		while (1)
		{
			u1 = uniform(seed);
			u2 = uniform(seed);
			v = a*log(u1/(1-u1));
			y = a1*exp(v);
			z = pow(u1,2)*u2;
			w = b + q*v - y;
			if ((w + d - theta*z) >= 0)
				return y;
			else if (w >= log(z))
				return y;
		}
		//should never get here
		error("Error in gamma_a1_1 (a1>1) \n");
		exit(0);
	}
}


double gamma_a1_a2(double a1, double a2, long int *seed)
{
	return (a2*gamma_a1_1(a1, seed));
}

//from Law and Kelton:
double beta(double a1, double a2, long int *seed)
{
	double gamma1, gamma2, retval;

	gamma1 = gamma_a1_1(a1, seed);
	gamma2 = gamma_a1_1(a2, seed);

	retval = gamma1/(gamma1 + gamma2);
	return retval;
}

