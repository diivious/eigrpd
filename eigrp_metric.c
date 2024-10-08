/*
 * EIGRP Metric Math Functions.
 * Copyright (C) 2013-2016
 * Authors:
 *   Donnie Savage
 */
#include "eigrpd/eigrpd.h"
#include "eigrpd/eigrp_structs.h"
#include "eigrpd/eigrp_metric.h"

eigrp_scaled_t eigrp_bandwidth_to_scaled(eigrp_bandwidth_t bandwidth)
{
	eigrp_bandwidth_t scaled = EIGRP_BANDWIDTH_MAX;

	if (bandwidth != EIGRP_BANDWIDTH_MAX) {
		scaled = (EIGRP_CLASSIC_SCALER * EIGRP_BANDWIDTH_SCALER);
		scaled = scaled / bandwidth;

		scaled = scaled ? scaled : EIGRP_BANDWIDTH_MIN;
	}

	scaled = (scaled < EIGRP_METRIC_MAX) ? scaled : EIGRP_METRIC_MAX;
	return (eigrp_scaled_t)scaled;
}

eigrp_bandwidth_t eigrp_scaled_to_bandwidth(eigrp_scaled_t scaled)
{
	eigrp_bandwidth_t bandwidth = EIGRP_BANDWIDTH_MAX;

	if (scaled != EIGRP_CLASSIC_MAX) {
		bandwidth = (EIGRP_CLASSIC_SCALER * EIGRP_BANDWIDTH_SCALER);
		bandwidth = scaled * bandwidth;
		bandwidth = (bandwidth < EIGRP_METRIC_MAX)
				    ? bandwidth
				    : EIGRP_BANDWIDTH_MAX;
	}

	return bandwidth;
}

eigrp_scaled_t eigrp_delay_to_scaled(eigrp_delay_t delay)
{
	delay = delay ? delay : EIGRP_DELAY_MIN;
	return delay * EIGRP_CLASSIC_SCALER;
}

eigrp_delay_t eigrp_scaled_to_delay(eigrp_scaled_t scaled)
{
	scaled = scaled / EIGRP_CLASSIC_SCALER;
	scaled = scaled ? scaled : EIGRP_DELAY_MIN;

	return scaled;
}

eigrp_metric_t eigrp_calculate_metrics(eigrp_instance_t *eigrp,
				       eigrp_metrics_t metric)
{
	eigrp_metric_t composite;
	composite = 0;

	if (metric.delay == EIGRP_METRIC_MAX)
		return EIGRP_METRIC_MAX;

	// EIGRP Composite =
	// {K1*BW+[(K2*BW)/(256-load)]+(K3*delay)}*{K5/(reliability+K4)}

	if (eigrp->k_values[0])
		composite += (eigrp->k_values[0] * metric.bandwidth);
	if (eigrp->k_values[1])
		composite += ((eigrp->k_values[1] * metric.bandwidth)
			      / (256 - metric.load));
	if (eigrp->k_values[2])
		composite += (eigrp->k_values[2] * metric.delay);
	if (eigrp->k_values[3] && !eigrp->k_values[4])
		composite *= eigrp->k_values[3];
	if (!eigrp->k_values[3] && eigrp->k_values[4])
		composite *= (eigrp->k_values[4] / metric.reliability);
	if (eigrp->k_values[3] && eigrp->k_values[4])
		composite *= ((eigrp->k_values[4] / metric.reliability)
			      + eigrp->k_values[3]);

	composite =
		(composite <= EIGRP_METRIC_MAX) ? composite : EIGRP_METRIC_MAX;

	return composite;
}

eigrp_metric_t eigrp_calculate_total_metrics(eigrp_instance_t *eigrp,
					     eigrp_route_descriptor_t *entry)
{
	eigrp_interface_t *ei = entry->ei;
	eigrp_delay_t temp_delay;
	eigrp_bandwidth_t bw;

	entry->total_metric = entry->reported_metric;
	temp_delay = entry->total_metric.delay
		     + eigrp_delay_to_scaled(ei->params.delay);

	entry->total_metric.delay =
		temp_delay > EIGRP_METRIC_MAX ? EIGRP_METRIC_MAX : temp_delay;

	bw = eigrp_bandwidth_to_scaled(ei->params.bandwidth);
	entry->total_metric.bandwidth = entry->total_metric.bandwidth > bw
						? bw
						: entry->total_metric.bandwidth;

	return eigrp_calculate_metrics(eigrp, entry->total_metric);
}

bool eigrp_metrics_is_same(eigrp_metrics_t metric1, eigrp_metrics_t metric2)
{
	if ((metric1.bandwidth == metric2.bandwidth)
	    && (metric1.delay == metric2.delay)
	    && (metric1.hop_count == metric2.hop_count)
	    && (metric1.load == metric2.load)
	    && (metric1.reliability == metric2.reliability)
	    && (metric1.mtu[0] == metric2.mtu[0])
	    && (metric1.mtu[1] == metric2.mtu[1])
	    && (metric1.mtu[2] == metric2.mtu[2])) {
		return TRUE;
	}

	return FALSE; // if different
}
