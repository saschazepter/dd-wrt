// SPDX-License-Identifier: GPL-2.0

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/cpufreq.h>
#include <linux/devfreq.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include <linux/pm_opp.h>

#include "governor.h"

struct krait_cache_data {
	struct clk *clk;
	unsigned long idle_freq;
	int token;
};

static int krait_cache_config_clk(struct device *dev, struct opp_table *opp_table,
			struct dev_pm_opp *old_opp, struct dev_pm_opp *opp,
			void *data, bool scaling_down)
{
	struct krait_cache_data *kdata;
	unsigned long old_freq, freq;
	unsigned long idle_freq;
	struct clk *clk;
	int ret;

	kdata = dev_get_drvdata(dev);
	idle_freq = kdata->idle_freq;
	clk = kdata->clk;

	old_freq = dev_pm_opp_get_freq(old_opp);
	freq = dev_pm_opp_get_freq(opp);

	/*
	 * Set to idle bin if switching from normal to high bin
	 * or vice versa. It has been notice that a bug is triggered
	 * in cache scaling when more than one bin is scaled, to fix
	 * this we first need to transition to the base rate and then
	 * to target rate
	 */
	if (likely(freq != idle_freq && old_freq != idle_freq)) {
		ret = clk_set_rate(clk, idle_freq);
		if (ret)
			return ret;
	}

	return clk_set_rate(clk, freq);
};

static int krait_cache_get_cur_freq(struct device *dev, unsigned long *freq)
{
	struct krait_cache_data *data = dev_get_drvdata(dev);

	*freq = clk_get_rate(data->clk);

	return 0;
};

static int krait_cache_target(struct device *dev, unsigned long *freq,
			      u32 flags)
{
	struct dev_pm_opp *opp;

	opp = dev_pm_opp_find_freq_ceil(dev, freq);
	if (unlikely(IS_ERR(opp)))
		return PTR_ERR(opp);

	dev_pm_opp_put(opp);

	return dev_pm_opp_set_rate(dev, *freq);
};

static int krait_cache_get_dev_status(struct device *dev,
				      struct devfreq_dev_status *stat)
{
	struct krait_cache_data *data = dev_get_drvdata(dev);

	stat->busy_time = 0;
	stat->total_time = 0;
	stat->current_frequency = clk_get_rate(data->clk);

	return 0;
};

static struct devfreq_dev_profile krait_cache_devfreq_profile = {
	.target = krait_cache_target,
	.get_dev_status = krait_cache_get_dev_status,
	.get_cur_freq = krait_cache_get_cur_freq
};

static struct devfreq_passive_data devfreq_gov_data = {
	.parent_type = CPUFREQ_PARENT_DEV,
};

static int krait_cache_probe(struct platform_device *pdev)
{
	struct dev_pm_opp_config config = { };
	struct device *dev = &pdev->dev;
	struct krait_cache_data *data;
	struct devfreq *devfreq;
	struct dev_pm_opp *opp;
	struct clk *clk;
	int ret, token;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	clk = devm_clk_get(dev, "l2");
	if (IS_ERR(clk))
		return PTR_ERR(clk);

	config.regulator_names = (const char *[]){ "l2", NULL };
	config.clk_names = (const char *[]){ "l2", NULL };
	config.config_clks = krait_cache_config_clk;

	token = dev_pm_opp_set_config(dev, &config);
	if (token < 0)
		return token;

	ret = devm_pm_opp_of_add_table(dev);
	if (ret)
		goto free_opp;

	opp = dev_pm_opp_find_freq_ceil(dev, &data->idle_freq);
	if (IS_ERR(opp)) {
		ret = PTR_ERR(opp);
		goto free_opp;
	}
	dev_pm_opp_put(opp);

	data->token = token;
	data->clk = clk;
	dev_set_drvdata(dev, data);
	devfreq = devm_devfreq_add_device(dev, &krait_cache_devfreq_profile,
					  DEVFREQ_GOV_PASSIVE, &devfreq_gov_data);
	if (IS_ERR(devfreq)) {
		ret = PTR_ERR(devfreq);
		goto free_opp;
	}

	return 0;

free_opp:
	dev_pm_opp_clear_config(token);
	return ret;
};

static int krait_cache_remove(struct platform_device *pdev)
{
	struct krait_cache_data *data = dev_get_drvdata(&pdev->dev);

	dev_pm_opp_clear_config(data->token);

	return 0;
};

static const struct of_device_id krait_cache_match_table[] = {
	{ .compatible = "qcom,krait-cache" },
	{}
};

static struct platform_driver krait_cache_driver = {
	.probe		= krait_cache_probe,
	.remove		= krait_cache_remove,
	.driver		= {
		.name   = "krait-cache-scaling",
		.of_match_table = krait_cache_match_table,
	},
};
module_platform_driver(krait_cache_driver);

MODULE_DESCRIPTION("Krait CPU Cache Scaling driver");
MODULE_AUTHOR("Christian Marangi <ansuelsmth@gmail.com>");
MODULE_LICENSE("GPL v2");
