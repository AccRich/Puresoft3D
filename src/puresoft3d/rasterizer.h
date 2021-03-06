#pragma once

class LineSegment;
class PuresoftRasterizer
{
public:
	typedef struct
	{
		float x, y;
	} VERTEX2F;

	typedef struct
	{
		int left;
		int leftClamped;
		int leftVerts[2];  // indices of verts for left point
		int right;
		int rightClamped;
		int rightVerts[2]; // indices of verts for right point
	} RESULT_ROW;

	typedef struct
	{
		VERTEX2F vertices[3];
		int firstRow;
		int lastRow;
		RESULT_ROW* m_rows;
	} RESULT;

public:
	PuresoftRasterizer(void);
	~PuresoftRasterizer(void);

	const RESULT* initialize(int width, int height);
	bool pushTriangle(const float* vert0, const float* vert1, const float* vert2);

private:
	int m_width;
	int m_height;
	int m_halfWidth;
	int m_halfHeight;
	int m_resultCapacity;
	RESULT m_output;

	inline void pushVertex(int idx, const float* vert);
	inline void processTriangle(const LineSegment& edgeL, const LineSegment& edgeR, float yMin, float yMax);
	inline void processStandingTriangle(const VERTEX2F* verts, int top, int bottom, int third);
};

