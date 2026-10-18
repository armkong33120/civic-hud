package com.civichud.maintenance.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.size
import androidx.compose.material3.LocalContentColor
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.unit.dp

@Composable
internal fun NavigationGlyph(index:Int) {
    val color=LocalContentColor.current
    Canvas(Modifier.size(24.dp)) {
        val unit=size.width/24f
        val stroke=Stroke(1.8f*unit)
        when(index) {
            0 -> {
                val path=Path().apply {moveTo(3*unit,11*unit);lineTo(12*unit,3*unit);lineTo(21*unit,11*unit);moveTo(5*unit,10*unit);lineTo(5*unit,21*unit);lineTo(19*unit,21*unit);lineTo(19*unit,10*unit)}
                drawPath(path,color,style=stroke)
                drawRoundRect(color,Offset(10*unit,14*unit),Size(4*unit,7*unit),CornerRadius(unit),style=stroke)
            }
            1 -> {
                drawRoundRect(color,Offset(5*unit,3*unit),Size(14*unit,18*unit),CornerRadius(2*unit),style=stroke)
                listOf(8,12,16).forEach {y->drawLine(color,Offset(8*unit,y*unit),Offset(16*unit,y*unit),strokeWidth=1.8f*unit)}
            }
            else -> {
                listOf(6,12,18).forEachIndexed {i,y->
                    drawLine(color,Offset(3*unit,y*unit),Offset(21*unit,y*unit),strokeWidth=1.8f*unit)
                    drawCircle(color,2.5f*unit,Offset((if(i==1) 9 else 16)*unit,y*unit),style=stroke)
                }
            }
        }
    }
}
