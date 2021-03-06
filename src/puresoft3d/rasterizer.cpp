#include <stdlib.h>
#include <stdexcept>
#include "rasterizer.h"

using namespace std;

PuresoftRasterizer::PuresoftRasterizer(void)
{
	m_width = 
	m_height = 
	m_halfWidth = 
	m_halfHeight = 
	m_resultCapacity = 0;
	memset(&m_output, 0, sizeof(m_output));
}


PuresoftRasterizer::~PuresoftRasterizer(void)
{
	if(m_output.m_rows)
	{
		delete[] m_output.m_rows;
	}
}

const PuresoftRasterizer::RESULT* PuresoftRasterizer::initialize(int width, int height)
{
	m_width = width;
	m_height = height;
	m_halfWidth = m_width / 2;
	m_halfHeight = m_height / 2;

	if(height > m_resultCapacity)
	{
		if(m_output.m_rows)
		{
			delete[] m_output.m_rows;
		}

		m_output.m_rows = new RESULT_ROW[m_resultCapacity = height];
	}

	return &m_output;
}

class LineSegment
{
	float dx, dy, x0, y0;
public:
	LineSegment(const PuresoftRasterizer::VERTEX2F* verts, int idx0, int idx1)
	{
		x0 = verts[idx0].x;
		y0 = verts[idx0].y;
		dx = verts[idx1].x - x0;
		dy = verts[idx1].y - y0;
		index0 = idx0;
		index1 = idx1;

		// this is to avoid division-by-0 in operator(), we would always get x0
		// rather than getting wrong float value
		if(fabs(dy) < 0.000001f)
			dy = FLT_MAX;
	}

	float operator() (float y) const
	{
		return dx * (y - y0) / dy + x0;
	}

	int index0, index1;
};

void PuresoftRasterizer::pushVertex(int idx, const float* vert)
{
	m_output.vertices[idx].y = ((float)m_halfHeight * vert[1]) + (float)m_halfHeight;
	m_output.vertices[idx].x = ((float)m_halfWidth  * vert[0]) + (float)m_halfWidth;

	if(0 == idx)
	{
		m_output.firstRow = m_output.lastRow = (int)m_output.vertices[0].y;
	}
	else if(m_output.vertices[idx].y > (float)m_output.lastRow)
	{
		m_output.lastRow = (int)m_output.vertices[idx].y;
	}
	else if(m_output.vertices[idx].y < (float)m_output.firstRow)
	{
		m_output.firstRow = (int)m_output.vertices[idx].y;
	}
}

// for flat top/bottom triangle
void PuresoftRasterizer::processTriangle(const LineSegment& edgeL, const LineSegment& edgeR, float yMin, float yMax)
{
	int iyMin = yMin < 0 ? 0 : (int)(yMin);
	int lastRowIdx = m_height - 1;
	int iyMax = yMax > (float)lastRowIdx ? lastRowIdx : (int)(yMax);
	for(; iyMin <= iyMax; iyMin++)
	{
		m_output.m_rows[iyMin].left = (int)(edgeL((float)iyMin) + 0.5f);

		if(m_output.m_rows[iyMin].left < 0)
			m_output.m_rows[iyMin].leftClamped = 0;
		else
			m_output.m_rows[iyMin].leftClamped = (int)m_output.m_rows[iyMin].left;

		m_output.m_rows[iyMin].right = (int)(edgeR((float)iyMin) + 0.5f);
		if(m_output.m_rows[iyMin].right >= m_width)
			m_output.m_rows[iyMin].rightClamped = m_width - 1;
		else
			m_output.m_rows[iyMin].rightClamped = m_output.m_rows[iyMin].right;

		m_output.m_rows[iyMin].leftVerts[0] = edgeL.index0;
		m_output.m_rows[iyMin].leftVerts[1] = edgeL.index1;
		m_output.m_rows[iyMin].rightVerts[0] = edgeR.index0;
		m_output.m_rows[iyMin].rightVerts[1] = edgeR.index1;
	}
}

// for non- flat top/bottom triangle, we split it to two flat top/bottom triangles
void PuresoftRasterizer::processStandingTriangle(const VERTEX2F* verts, int top, int bottom, int third)
{
	LineSegment edgeTopThird(verts, top, third), edgeTopBottom(verts, top, bottom), edgeThirdBottom(verts, third, bottom);
	
	// the triangle is pointing left
	if(edgeTopBottom(verts[third].y) > edgeTopThird(verts[third].y))
	{
		// upper half
		processTriangle(edgeTopThird, edgeTopBottom, verts[third].y, verts[top].y);
		// lower half
		processTriangle(edgeThirdBottom, edgeTopBottom, verts[bottom].y, verts[third].y);
	}
	else
	{
		// upper half
		processTriangle(edgeTopBottom, edgeTopThird, verts[third].y, verts[top].y);
		// lower half
		processTriangle(edgeTopBottom, edgeThirdBottom, verts[bottom].y, verts[third].y);
	}
}

bool PuresoftRasterizer::pushTriangle(const float* vert0, const float* vert1, const float* vert2)
{
	// integerize
	pushVertex(0, vert0);
	pushVertex(1, vert1);
	pushVertex(2, vert2);

	if(m_output.firstRow >= m_height || m_output.lastRow < 0)
	{
		m_output.firstRow = 0;
		m_output.lastRow = -1;
	}

	if(m_output.firstRow < 0)
	{
		m_output.firstRow = 0;
	}

	if(m_output.lastRow >= m_height)
	{
		m_output.lastRow = m_height - 1;
	}

	if(m_output.firstRow == m_output.lastRow)
	{
		return false;
	}

	// flat top/bottom
	if(m_output.vertices[0].y == m_output.vertices[1].y)
	{
		if(m_output.vertices[0].x < m_output.vertices[1].x)
		{
			processTriangle(LineSegment(m_output.vertices, 0, 2), LineSegment(m_output.vertices, 1, 2), (float)m_output.firstRow, (float)m_output.lastRow);
		}
		else
		{
			processTriangle(LineSegment(m_output.vertices, 1, 2), LineSegment(m_output.vertices, 0, 2), (float)m_output.firstRow, (float)m_output.lastRow);
		}
	}
	else if(m_output.vertices[0].y == m_output.vertices[2].y)
	{
		if(m_output.vertices[0].x < m_output.vertices[2].x)
		{
			processTriangle(LineSegment(m_output.vertices, 0, 1), LineSegment(m_output.vertices, 2, 1), (float)m_output.firstRow, (float)m_output.lastRow);
		}
		else
		{
			processTriangle(LineSegment(m_output.vertices, 2, 1), LineSegment(m_output.vertices, 0, 1), (float)m_output.firstRow, (float)m_output.lastRow);
		}
	}
	else if(m_output.vertices[1].y == m_output.vertices[2].y)
	{
		if(m_output.vertices[1].x < m_output.vertices[2].x)
		{
			processTriangle(LineSegment(m_output.vertices, 1, 0), LineSegment(m_output.vertices, 2, 0), (float)m_output.firstRow, (float)m_output.lastRow);
		}
		else
		{
			processTriangle(LineSegment(m_output.vertices, 2, 0), LineSegment(m_output.vertices, 1, 0), (float)m_output.firstRow, (float)m_output.lastRow);
		}
	}
	// non- flat top/bottom
	else
	{
		// sort the 3 Ys
		int top, bottom, third = 2;

		if(m_output.vertices[0].y > m_output.vertices[1].y)
		{
			top = 0;
			bottom = 1;
		}
		else
		{
			top = 1;
			bottom = 0;
		}

		if(m_output.vertices[top].y < m_output.vertices[third].y)
		{
			top ^= third, third ^= top, top ^= third;
		}
		else if(m_output.vertices[bottom].y > m_output.vertices[third].y)
		{
			bottom ^= third, third ^= bottom, bottom ^= third;
		}

		processStandingTriangle(m_output.vertices, top, bottom, third);
	}

	return true;
}
