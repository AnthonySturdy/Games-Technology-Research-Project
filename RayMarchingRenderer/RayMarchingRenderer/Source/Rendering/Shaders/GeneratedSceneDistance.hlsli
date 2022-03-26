float sdfSphere(float3 p, float3 param){
	return length(p) - param.x;
}

float GetDistanceToScene(float3 p, out int index)
{
    float dist = renderSettings.maxDist; 
	int objIndex = 0;

	dist = min(dist, sdfSphere(p - ObjectsList[0].Position, ObjectsList[0].Parameters));
	dist = min(dist, sdfSphere(p - ObjectsList[1].Position, ObjectsList[1].Parameters));

	index = objIndex; 
	return dist;
}