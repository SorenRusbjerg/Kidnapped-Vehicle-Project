/**
 * particle_filter.cpp
 *
 * Created on: Dec 12, 2016
 * Author: Tiffany Huang
 */

#include "particle_filter.h"

#include <math.h>
#include <algorithm>
#include <iostream>
#include <iterator>
#include <numeric>
#include <random>
#include <string>
#include <vector>

#include "helper_functions.h"

using std::string;
using std::vector;
using std::normal_distribution;


void ParticleFilter::init(double x, double y, double theta, double std[]) {
  /**
   *  Set the number of particles. Initialize all particles to 
   *   first position (based on estimates of x, y, theta and their uncertainties
   *   from GPS) and all weights to 1. 
   *  Add random Gaussian noise to each particle.
   * NOTE: Consult particle_filter.h for more information about this method 
   *   (and others in this file).
   */
  num_particles = 100;  //  Set the number of particles

  // This line creates a normal (Gaussian) distribution for x, y and theta
  std::default_random_engine randGen;
  normal_distribution<double> dist_x(x, std[0]);
  normal_distribution<double> dist_y(y, std[1]);
  normal_distribution<double> dist_theta(theta, std[2]);

  for (int n=0; n<num_particles; n++)
  {
    Particle newParticle;

    newParticle.x = dist_x(randGen);
    newParticle.y = dist_y(randGen);
    newParticle.theta = dist_theta(randGen);
    newParticle.weight = 1.0;
    newParticle.id = n;

    particles.push_back(newParticle);    
  } 

  is_initialized = true;

}

void ParticleFilter::prediction(double delta_t, double std_pos[], 
                                double velocity, double yaw_rate) {
  /**
   * Add measurements to each particle and add random Gaussian noise.
   * NOTE: When adding noise you may find std::normal_distribution 
   *   and std::default_random_engine useful.
   *  http://en.cppreference.com/w/cpp/numeric/random/normal_distribution
   *  http://www.cplusplus.com/reference/random/default_random_engine/
   */
  // This line creates a normal (Gaussian) distribution for x, y and theta
  std::default_random_engine randGen;
  normal_distribution<double> dist_x(0, std_pos[0]);
  normal_distribution<double> dist_y(0, std_pos[1]);
  normal_distribution<double> dist_theta(0, std_pos[2]);

  for (int n=0; n<num_particles; n++)
  {
    if (abs(yaw_rate) > 0.0001)
    {
      particles[n].x += velocity / yaw_rate * (sin(particles[n].theta + yaw_rate * delta_t) - sin(particles[n].theta)) + dist_x(randGen);
      particles[n].y += velocity / yaw_rate * (-cos(particles[n].theta + yaw_rate * delta_t) + cos(particles[n].theta)) + dist_y(randGen);
      particles[n].theta += yaw_rate * delta_t + dist_theta(randGen);
    }
    else // for small yaw_rate
    {
      particles[n].x += velocity*cos(particles[n].theta)*delta_t + dist_x(randGen);
      particles[n].y += velocity*sin(particles[n].theta)*delta_t + dist_y(randGen);
      particles[n].theta += yaw_rate * delta_t + dist_theta(randGen);      
    }    
  } 
}

void ParticleFilter::dataAssociation(vector<LandmarkObs> predicted, 
                                     vector<LandmarkObs>& observations) {
  /**
   * Find the predicted measurement that is closest to each 
   *   observed measurement and assign the observed measurement to this 
   *   particular landmark.
   * NOTE: this method will NOT be called by the grading code. But you will 
   *   probably find it useful to implement this method and use it as a helper 
   *   during the updateWeights phase.
   */
  for (auto& obs : observations)
  {
    double minDist = 9.9e50; // large number
    LandmarkObs bestFit;
    for (auto pred : predicted)
    {
      double d = dist(pred.x, pred.y, obs.x, obs.y); 
      if (d < minDist)
      {
        minDist = d;
        bestFit = pred;
      }
    } 
    obs.id = bestFit.id;      
  }

}

  /**
   *  Calculate a single particle weight from observed measurements in 'particle' and 
   * predicted landmarks from map. All is in map coordinates. 
   */
  void ParticleFilter::CalculateParticleWeight(Particle& particle, double std_landmark[], const vector<LandmarkObs> &predictedLandMarks) 
  {

  double weight = 1.0;  // particle weight

  // Loop through associations and calcualte partial probability
  for (uint n=0; n < particle.associations.size(); n++)
  {
    int lmId = particle.associations[n];

    LandmarkObs pred_LM_match;
    // Find LM association in 
    for (auto pred_LM : predictedLandMarks)
    {
      if (lmId == pred_LM.id)
      {
        pred_LM_match = pred_LM;
        break;
      }
    }
    
    // Calculate probability
    weight *= multiv_prob(std_landmark[0], std_landmark[1], particle.sense_x[n], particle.sense_y[n], pred_LM_match.x, pred_LM_match.y);
  }

  // Update weight  
  particle.weight = weight;
}

void ParticleFilter::updateWeights(double sensor_range, double std_landmark[], 
                                   const vector<LandmarkObs> &observations, 
                                   const Map &map_landmarks) {
  /**
   * Update the weights of each particle using a mult-variate Gaussian 
   *   distribution. You can read more about this distribution here: 
   *   https://en.wikipedia.org/wiki/Multivariate_normal_distribution
   * NOTE: The observations are given in the VEHICLE'S coordinate system. 
   *   Your particles are located according to the MAP'S coordinate system. 
   *   You will need to transform between the two systems. Keep in mind that
   *   this transformation requires both rotation AND translation (but no scaling).
   *   The following is a good resource for the theory:
   *   https://www.willamette.edu/~gorr/classes/GeneralGraphics/Transforms/transforms2d.htm
   *   and the following is a good resource for the actual equation to implement
   *   (look at equation 3.33) http://planning.cs.uiuc.edu/node99.html
   */

  vector<LandmarkObs> observations_mapCoordinates;
  vector<LandmarkObs> predictedLMs;

  for (int n=0; n<num_particles; n++)
  { 
    // Clear observations and predicted landmarks for new particle
    observations_mapCoordinates.clear();
    predictedLMs.clear();
 
    // Get observations in map coordinates
    for (uint m=0; m<observations.size(); m++)
    {
      LandmarkObs obs_lm;
      obs_lm.x = particles[n].x + cos(particles[n].theta)*observations[m].x - sin(particles[n].theta)*observations[m].y;
      obs_lm.y = particles[n].y + sin(particles[n].theta)*observations[m].x + cos(particles[n].theta)*observations[m].y; 
      obs_lm.id = observations[m].id;
      observations_mapCoordinates.push_back(obs_lm);
    }

    // Find nearest predicted landmarks to particles within sensor range
    for (auto landmark : map_landmarks.landmark_list)
    {
      LandmarkObs lm;
      lm.id = landmark.id_i;
      lm.x = landmark.x_f;
      lm.y = landmark.y_f;

      double d = dist(lm.x, lm.y, particles[n].x, particles[n].y); 
      if (d < sensor_range)
      {
        predictedLMs.push_back(lm);
      }
    }

    // Associate observations with predicted landmarks
    dataAssociation(predictedLMs, observations_mapCoordinates);

    // Copy data from observations into particle
    particles[n].sense_x.clear();
    particles[n].sense_y.clear();
    particles[n].associations.clear();
    for (auto obs : observations_mapCoordinates)
    {
      particles[n].sense_x.push_back(obs.x);
      particles[n].sense_y.push_back(obs.y);
      particles[n].associations.push_back(obs.id);    
    }    
    
    CalculateParticleWeight(particles[n], std_landmark, predictedLMs); 
  }

  // Normalize weights
  double sum = 0.0;
  for (int n = 0; n < num_particles; n++)
  {
    sum += particles[n].weight;
  }
  if (sum > 1e-5)  // avoid divide by 0
  {
    for (int n = 0; n < num_particles; n++)
    {
      particles[n].weight = particles[n].weight / sum;
    }
  }
}

void ParticleFilter::resample() {
  /**
   * Resample particles with replacement with probability proportional 
   *   to their weight. 
   * NOTE: You may find std::discrete_distribution helpful here.
   *   http://en.cppreference.com/w/cpp/numeric/random/discrete_distribution
   */
  std::vector<Particle> newParticles;
  std::default_random_engine gen;
  vector<double> weights;
  // Generate vector with all weights
  for (int n=0; n<num_particles; n++)
  {
    weights.push_back(particles[n].weight);
  }

  // Use discrete_distribution to sample with probability proportional to their weight.  
  std::discrete_distribution<int> particleDistr(weights.begin(),weights.end());

  // Generate vector with new particles
  for (int n=0; n<num_particles; n++)
  {
    int pIdx = particleDistr(gen);
    newParticles.push_back(particles[pIdx]);  // resamble
  }

  // overwrite old particles
  particles = newParticles;

}

void ParticleFilter::SetAssociations(Particle& particle, 
                                     const vector<int>& associations, 
                                     const vector<double>& sense_x, 
                                     const vector<double>& sense_y) {
  // particle: the particle to which assign each listed association, 
  //   and association's (x,y) world coordinates mapping
  // associations: The landmark id that goes along with each listed association
  // sense_x: the associations x mapping already converted to world coordinates
  // sense_y: the associations y mapping already converted to world coordinates
  particle.associations= associations;
  particle.sense_x = sense_x;
  particle.sense_y = sense_y;
}

string ParticleFilter::getAssociations(Particle best) {
  vector<int> v = best.associations;
  std::stringstream ss;
  copy(v.begin(), v.end(), std::ostream_iterator<int>(ss, " "));
  string s = ss.str();
  s = s.substr(0, s.length()-1);  // get rid of the trailing space
  return s;
}

string ParticleFilter::getSenseCoord(Particle best, string coord) {
  vector<double> v;

  if (coord == "X") {
    v = best.sense_x;
  } else {
    v = best.sense_y;
  }

  std::stringstream ss;
  copy(v.begin(), v.end(), std::ostream_iterator<float>(ss, " "));
  string s = ss.str();
  s = s.substr(0, s.length()-1);  // get rid of the trailing space
  return s;

}

    /**
   * Print particle data to console.
   */
  void ParticleFilter::PrintParticleData(const Particle& particle, std::fstream& fileStream)
  {
    string ass = string(getAssociations(particle));
    fileStream << "\nParticle " << particle.id << "\nXpos: " << particle.x << "\nYpos: " << particle.y
         << "\nTheta: " << particle.theta << "\nWeight: " << particle.weight
         << "\nAssociations: " << ass.c_str() << std::endl;        
  }


  /**
   * Print all particles data to console.
   */
  void ParticleFilter::PrintAllParticlesData(std::fstream& fileStream)
  {
    for (int n=0; n<num_particles; n++)
    {
      PrintParticleData(particles[n], fileStream);
    }
    fileStream << "=======================================================\n";
    fileStream.flush();
  }
