[timeout:90];
(
  way["highway"~"^(motorway|trunk|primary|secondary|tertiary|motorway_link|trunk_link|primary_link|secondary_link|tertiary_link|unclassified)$"](10.760,106.650,10.825,106.730);
);
out body;
>;
out skel qt;
